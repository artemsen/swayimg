// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "sway.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <json.h>

/** IPC magic header value */
static const uint8_t ipc_magic[] = { 'i', '3', '-', 'i', 'p', 'c' };

/** IPC message types (used only) */
enum ipc_msg_type {
    IPC_COMMAND = 0,
    IPC_GET_WORKSPACES = 1,
    IPC_GET_TREE = 4
};

/** IPC header */
struct __attribute__((__packed__)) ipc_header {
    uint8_t magic[sizeof(ipc_magic)];
    uint32_t len;
    uint32_t type;
};

/**
 * Read exactly specified number of bytes from socket.
 * @param[in] fd socket descriptor
 * @param[out] buf buffer for destination data
 * @param[in] len number of bytes to read
 * @return true if operation completed successfully
 */
static bool sock_read(int fd, void* buf, size_t len)
{
    while (len) {
        const ssize_t rcv = recv(fd, buf, len, 0);
        if (rcv == 0) {
            fprintf(stderr, "No data in IPC channel\n");
            return false;
        }
        if (rcv == -1) {
            fprintf(stderr, "Unable to read IPC socket: [%i] %s\n", errno,
                    strerror(errno));
            return false;
        }
        len -= rcv;
        buf = ((uint8_t*)buf) + rcv;
    }
    return true;
}

/**
 * Write data to the socket.
 * @param[in] fd socket descriptor
 * @param[in] buf buffer of data of send
 * @param[in] len number of bytes to write
 * @return true if operation completed successfully
 */
static bool sock_write(int fd, const void* buf, size_t len)
{
    while (len) {
        const ssize_t rcv = write(fd, buf, len);
        if (rcv == -1) {
            fprintf(stderr, "Unable to write IPC socket: [%i] %s\n", errno,
                    strerror(errno));
            return false;
        }
        len -= rcv;
        buf = ((uint8_t*)buf) + rcv;
    }
    return true;
}

/**
 * @brief IPC message exchange.
 * @param[in] ipc IPC context (socket file descriptor)
 * @param[in] type message type
 * @param[in] payload payload data
 * @return IPC response as json object, NULL on errors
 */
static struct json_object* ipc_message(int ipc, enum ipc_msg_type type,
        const char* payload)
{
    struct ipc_header hdr;
    memcpy(hdr.magic, ipc_magic, sizeof(ipc_magic));
    hdr.len = payload ? strlen(payload) : 0;
    hdr.type = type;

    // send request
    if (!sock_write(ipc, &hdr, sizeof(hdr)) ||
        !sock_write(ipc, payload, hdr.len)) {
        return NULL;
    }

    // receive response
    if (!sock_read(ipc, &hdr, sizeof(hdr))) {
        return NULL;
    }
    char* raw = malloc(hdr.len + 1);
    if (!raw) {
        fprintf(stderr, "Not enough memory\n");
        return NULL;
    }
    if (!sock_read(ipc, raw, hdr.len)) {
        free(raw);
        return NULL;
    }
    raw[hdr.len] = 0;

    struct json_object* response = json_tokener_parse(raw);
    if (!response) {
        fprintf(stderr, "Invalid IPC response\n");
    }
    
    free(raw);

    return response;
}

/**
 * Send command for specified application.
 * @param[in] ipc IPC context (socket file descriptor)
 * @param[in] app application Id
 * @param[in] command command to send
 * @return true if operation completed successfully
 */
static bool ipc_command(int ipc, const char* app, const char* command)
{
    bool rc = false;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "for_window [app_id=%s] %s", app, command);

    json_object* response = ipc_message(ipc, IPC_COMMAND, cmd);
    if (response) {
        struct json_object* val = json_object_array_get_idx(response, 0);
        if (val) {
            rc = json_object_object_get_ex(val, "success", &val) &&
                json_object_get_boolean(val);
        }
        if (!rc) {
            fprintf(stderr, "Bad IPC response\n");
        }
        json_object_put(response);
    }

    return rc;
}

/**
 * Read rectange geometry from JSON node.
 * @param[in] node JSON parent node
 * @param[in] name name of the rect node
 * @param[out] rect rectangle geometry
 * @return true if operation completed successfully
 */
static bool read_rect(json_object* node, const char* name, struct rect* rect)
{
    struct json_object* rect_node;
    if (!json_object_object_get_ex(node, name, &rect_node)) {
        fprintf(stderr, "Failed to read rect: node %s not found\n", name);
        return false;
    }

    struct rdata {
        const char* name;
        int* val;
    } const rdata[] = {
        { "x",      &rect->x },
        { "y",      &rect->y },
        { "width",  &rect->width },
        { "height", &rect->height },
    };

    for (size_t i = 0; i < sizeof(rdata) / sizeof(rdata[0]); ++i) {
        struct json_object* val;
        if (!json_object_object_get_ex(rect_node, rdata[i].name, &val)) {
            fprintf(stderr, "Failed to read rect: field %s not found\n",
                    rdata[i].name);
            return false;
        }
        *rdata[i].val = json_object_get_int(val);
        if (*rdata[i].val == 0 && errno == EINVAL) {
            fprintf(stderr,
                    "Failed to read rect: field %s has invalid format\n",
                    rdata[i].name);
            return false;
        }
    }

    return true;
}

/**
 * Get geometry for currently focused workspace.
 * @param[in] ipc IPC context (socket file descriptor)
 * @param[out] workspace rectangle geometry
 * @return true if operation completed successfully
 */
static bool workspace_geometry(int ipc, struct rect* workspace)
{
    bool found = false;

    json_object* response = ipc_message(ipc, IPC_GET_WORKSPACES, NULL);
    if (response) {
        int idx = json_object_array_length(response);
        while (!found && --idx >= 0) {
            struct json_object* wks;
            struct json_object* focused;
            wks = json_object_array_get_idx(response, idx);
            found = json_object_object_get_ex(wks, "focused", &focused) &&
                    json_object_get_boolean(focused) &&
                    read_rect(wks, "rect", workspace);
        }
        json_object_put(response);
    }

    return found;
}

/**
 * Get currently focused window node.
 * @param[in] node parent JSON node
 * @return pointer to focused window node or NULL if not found
 */
static struct json_object* get_focused(json_object* node)
{
    struct json_object* focused;
    if (json_object_object_get_ex(node, "focused", &focused) &&
        json_object_get_boolean(focused)) {
        return node;
    }

    struct json_object* nodes;
    if (json_object_object_get_ex(node, "nodes", &nodes)) {
        int idx = json_object_array_length(nodes);
        while (--idx >= 0) {
            struct json_object* sub = json_object_array_get_idx(nodes, idx);
            struct json_object* focus = get_focused(sub);
            if (focus) {
                return focus;
            }
        }
    }

    return NULL;
}

int sway_connect(void)
{
    struct sockaddr_un sa;
    memset(&sa, 0, sizeof(sa));

    const char* path = getenv("SWAYSOCK");
    if (!path) {
        fprintf(stderr, "SWAYSOCK variable is not defined\n");
        return -1;
    }
    size_t len = strlen(path);
    if (!len || len > sizeof(sa.sun_path)) {
        fprintf(stderr, "Invalid SWAYSOCK variable\n");
        return -1;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        fprintf(stderr, "Failed to create IPC socket\n");
        return -1;
    }

    sa.sun_family = AF_UNIX;
    memcpy(sa.sun_path, path, len);

    len += sizeof(sa) - sizeof(sa.sun_path);
    if (connect(fd, (struct sockaddr*)&sa, len) == -1) {
        fprintf(stderr, "Failed to connect IPC socket\n");
        close(fd);
        return -1;
    }

    return fd;
}

void sway_disconnect(int ipc)
{
    close(ipc);
}

bool sway_get_focused(int ipc, struct rect* rect)
{
    bool rc = false;

    json_object* response = ipc_message(ipc, IPC_GET_TREE, NULL);
    if (response) {
        struct json_object* focus = get_focused(response);
        if (focus) {
            struct rect workspace;
            if (workspace_geometry(ipc, &workspace)) {
                struct rect global;
                struct rect window;
                if (read_rect(focus, "rect", &global) &&
                    read_rect(focus, "window_rect", &window)) {
                    rect->x = global.x + window.x - workspace.x;
                    rect->y = global.y + window.y - workspace.y;
                    rect->width = window.width;
                    rect->height = window.height;
                    rc = true;
                }
            }
        }
        json_object_put(response);
    }

    return rc;
}

bool sway_add_rules(int ipc, const char* app, int x, int y)
{
    char move[64];
    snprintf(move, sizeof(move), "move position %i %i", x, y);
    return ipc_command(ipc, app, "floating enable") &&
           ipc_command(ipc, app, move);
}
