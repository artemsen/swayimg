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
            fprintf(stderr, "IPC error: no data\n");
            return false;
        }
        if (rcv == -1) {
            const int ec = errno;
            fprintf(stderr, "IPC read error: [%i] %s\n", ec, strerror(ec));
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
            const int ec = errno;
            fprintf(stderr, "IPC write error: [%i] %s\n", ec, strerror(ec));
            return false;
        }
        len -= rcv;
        buf = ((uint8_t*)buf) + rcv;
    }
    return true;
}

/**
 * IPC message exchange.
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
 * Read numeric value from JSON node.
 * @param[in] node JSON parent node
 * @param[in] name name of the rect node
 * @param[out] value value from JSON field
 * @return true if operation completed successfully
 */
static bool read_int(json_object* node, const char* name, int* value)
{
    struct json_object* val;
    if (!json_object_object_get_ex(node, name, &val)) {
        fprintf(stderr, "JSON scheme error: field %s not found\n", name);
        return false;
    }
    *value = json_object_get_int(val);
    if (*value == 0 && errno == EINVAL) {
        fprintf(stderr, "JSON scheme error: field %s not a number\n", name);
        return false;
    }
    return true;
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
    struct json_object* rn;
    if (!json_object_object_get_ex(node, name, &rn)) {
        fprintf(stderr, "Failed to read rect: node %s not found\n", name);
        return false;
    }
    return read_int(rn, "x", &rect->x) &&
           read_int(rn, "y", &rect->y) &&
           read_int(rn, "width", &rect->width) &&
           read_int(rn, "height", &rect->height);
}

/**
 * Get currently focused workspace.
 * @param[in] node parent JSON node
 * @return pointer to focused workspace node or NULL if not found
 */
static struct json_object* current_workspace(json_object* node)
{
    int idx = json_object_array_length(node);
    while (--idx >= 0) {
        struct json_object* focused;
        struct json_object* wks = json_object_array_get_idx(node, idx);
        if (json_object_object_get_ex(wks, "focused", &focused) &&
            json_object_get_boolean(focused)) {
            return wks;
        }
    }
    return NULL;
}

/**
 * Get currently focused window node.
 * @param[in] node parent JSON node
 * @return pointer to focused window node or NULL if not found
 */
static struct json_object* current_window(json_object* node)
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
            struct json_object* focus = current_window(sub);
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
        const int ec = errno;
        fprintf(stderr, "Failed to create IPC socket: [%i] %s\n", ec, strerror(ec));
        return -1;
    }

    sa.sun_family = AF_UNIX;
    memcpy(sa.sun_path, path, len);

    len += sizeof(sa) - sizeof(sa.sun_path);
    if (connect(fd, (struct sockaddr*)&sa, len) == -1) {
        const int ec = errno;
        fprintf(stderr, "Failed to connect IPC socket: [%i] %s\n", ec, strerror(ec));
        close(fd);
        return -1;
    }

    return fd;
}

void sway_disconnect(int ipc)
{
    close(ipc);
}

bool sway_current(int ipc, struct rect* wnd, bool* fullscreen)
{
    bool rc = false;

    // get currently focused window
    json_object* tree = ipc_message(ipc, IPC_GET_TREE, NULL);
    if (!tree) {
        return false;
    }
    json_object* cur_wnd = current_window(tree);
    if (!cur_wnd || !read_rect(cur_wnd, "window_rect", wnd)) {
        goto done;
    }

    // get full screen mode flag
    int fs_mode;
    *fullscreen = read_int(cur_wnd, "fullscreen_mode", &fs_mode) && fs_mode;
    if (*fullscreen) {
        rc = true;
        goto done;
    }

    // if we are not in the full screen mode - calculate client area offset
    json_object* workspaces = ipc_message(ipc, IPC_GET_WORKSPACES, NULL);
    if (!workspaces) {
        goto done;
    }
    json_object* cur_wks = current_workspace(workspaces);
    if (cur_wks) {
        struct rect workspace;
        struct rect global;
        rc = read_rect(cur_wks, "rect", &workspace) &&
             read_rect(cur_wnd, "rect", &global);
        if (rc) {
            wnd->x += global.x - workspace.x;
            wnd->y += global.y - workspace.y;
        }
    }
    json_object_put(workspaces);

done:
    json_object_put(tree);
    return rc;
}

bool sway_add_rules(int ipc, const char* app, int x, int y)
{
    char move[64];
    snprintf(move, sizeof(move), "move position %i %i", x, y);
    return ipc_command(ipc, app, "floating enable") &&
           ipc_command(ipc, app, move);
}
