// SPDX-License-Identifier: MIT
// Integration with Wayland compositor (Sway only).
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "compositor.h"

#include "array.h"

#include <assert.h>
#include <errno.h>
#include <json.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

// Invalid socket handle
#define INVALID_SOCKET -1

/** Sway IPC magic header value */
static const uint8_t sway_magic[] = { 'i', '3', '-', 'i', 'p', 'c' };

/** Sway IPC message types (used only) */
enum sway_msg_type {
    sway_run_command = 0,
    sway_get_tree = 4,
};

/** Sway IPC header */
struct __attribute__((__packed__)) sway_msg_header {
    uint8_t magic[sizeof(sway_magic)];
    uint32_t len;
    uint32_t type;
};

/**
 * Connect to UNIX socket.
 * @param path path to the socket file
 * @return socket descriptor or INVALID_SOCKET on errors
 */
static int sock_connect(const char* path)
{
    struct sockaddr_un sa = { .sun_family = AF_UNIX };
    const size_t len = path ? strlen(path) + 1 : 0;
    int fd;

    if (len == 0 || len > sizeof(sa.sun_path)) {
        fprintf(stderr, "Invalid IPC socket path\n");
        return INVALID_SOCKET;
    }
    memcpy(sa.sun_path, path, len);

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        const int rc = errno;
        fprintf(stderr, "Failed to open IPC socket %s: [%d] %s\n", path, rc,
                strerror(rc));
        return INVALID_SOCKET;
    }

    if (connect(fd, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
        const int rc = errno;
        fprintf(stderr, "Failed to connect IPC socket %s: [%d] %s\n", path, rc,
                strerror(rc));
        close(fd);
        return INVALID_SOCKET;
    }

    return fd;
}

/**
 * Read data from socket connection.
 * @param fd socket descriptor
 * @param buf buffer for destination data
 * @param size max number of bytes to read
 * @return number of bytes read
 */
static size_t sock_read(int fd, void* buffer, size_t size)
{
    size_t read = 0;

    while (read < size) {
        const ssize_t rc = recv(fd, ((uint8_t*)buffer) + read, size - read, 0);
        if (rc == 0) {
            break;
        }
        if (rc == -1) {
            const int ec = errno;
            fprintf(stderr, "IPC read error: [%i] %s\n", ec, strerror(ec));
            break;
        }
        read += rc;
    }

    if (read && read < size) {
        ((uint8_t*)buffer)[read] = 0; // add last null
    }

    return read;
}

/**
 * Write data to the socket.
 * @param fd socket descriptor
 * @param buf buffer of data of send
 * @param len number of bytes to write
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
 * Read numeric value from JSON node.
 * @param node JSON parent node
 * @param name name of the child node
 * @param value value from JSON field
 * @return true if operation completed successfully
 */
static bool read_jint(json_object* node, const char* name, int32_t* value)
{
    if (!node) {
        return false;
    }
    if (name) {
        struct json_object* obj;
        if (!json_object_object_get_ex(node, name, &obj)) {
            fprintf(stderr, "JSON scheme error: field %s not found\n", name);
            return false;
        }
        node = obj;
    }
    *value = json_object_get_int(node);
    if (*value == 0 && errno == EINVAL) {
        fprintf(stderr, "JSON scheme error: field %s not a number\n", name);
        return false;
    }
    return true;
}

/**
 * Connect to Sway IPC.
 * @return socket file descriptor or INVALID_SOCKET on errors
 */
static int sway_connect(void)
{
    const char* path = getenv("SWAYSOCK");
    return path && *path ? sock_connect(path) : INVALID_SOCKET;
}

/**
 * Get currently focused window node.
 * @param node parent JSON node
 * @return pointer to focused window node or NULL if not found
 */
static json_object* sway_find_focused(json_object* node)
{
    const char* node_names[] = { "nodes", "floating_nodes" };

    json_object* focused;
    if (json_object_object_get_ex(node, "focused", &focused) &&
        json_object_get_boolean(focused)) {
        return node;
    }

    for (size_t i = 0; i < ARRAY_SIZE(node_names); ++i) {
        json_object* nodes;
        if (json_object_object_get_ex(node, node_names[i], &nodes)) {
            const size_t size = json_object_array_length(nodes);
            for (size_t j = 0; j < size; ++j) {
                json_object* sub = json_object_array_get_idx(nodes, j);
                json_object* focus = sway_find_focused(sub);
                if (focus) {
                    return focus;
                }
            }
        }
    }

    return NULL;
}

/**
 * Sway IPC message exchange.
 * @param fd socket file descriptor
 * @param type message type
 * @param payload payload data
 * @return response as JSON object, NULL on errors
 */
static json_object* sway_request(int fd, enum sway_msg_type type,
                                 const char* payload)
{
    json_object* response = NULL;
    char* buffer = NULL;
    struct sway_msg_header hdr;

    memcpy(hdr.magic, sway_magic, sizeof(sway_magic));
    hdr.len = payload ? strlen(payload) : 0;
    hdr.type = type;

    // message exchange
    if (!sock_write(fd, &hdr, sizeof(hdr)) ||
        !sock_write(fd, payload, hdr.len) ||
        !sock_read(fd, &hdr, sizeof(hdr))) {
        goto done;
    }
    buffer = malloc(hdr.len + 1);
    if (!buffer) {
        goto done;
    }
    if (!sock_read(fd, buffer, hdr.len)) {
        goto done;
    }

    buffer[hdr.len] = 0;
    response = json_tokener_parse(buffer);

done:
    free(buffer);
    return response;
}

/** Sway: get geometry of currently focused window. */
static bool sway_get_focus(struct wndrect* wnd)
{
    bool rc = false;
    json_object* tree = NULL;
    json_object* focus;
    json_object* rect;
    json_object* rectwnd;
    int fd;

    fd = sway_connect();
    if (fd == INVALID_SOCKET) {
        return false;
    }

    // get currently focused window
    tree = sway_request(fd, sway_get_tree, NULL);
    if (!tree) {
        goto done;
    }
    focus = sway_find_focused(tree);
    if (!focus) {
        goto done;
    }

    // get window size and position
    if (json_object_object_get_ex(focus, "rect", &rect) &&
        json_object_object_get_ex(focus, "window_rect", &rectwnd)) {
        int32_t x, y, width, height, x_offset, y_offset;
        if (read_jint(rect, "x", &x) && read_jint(rect, "y", &y) &&
            read_jint(rectwnd, "x", &x_offset) &&
            read_jint(rectwnd, "y", &y_offset) &&
            read_jint(rectwnd, "width", &width) && width > 0 &&
            read_jint(rectwnd, "height", &height) && height > 0) {
            wnd->x = x + x_offset;
            wnd->y = y + y_offset;
            wnd->width = width;
            wnd->height = height;
            rc = true;
        }
    }

done:
    close(fd);
    json_object_put(tree);
    return rc;
}

/** Sway: Set rules to create overlay window. */
static bool sway_overlay(const struct wndrect* wnd)
{
    bool rc = false;
    const pid_t pid = getpid();
    json_object* response = NULL;
    char cmd[128];
    int fd;

    fd = sway_connect();
    if (fd == INVALID_SOCKET) {
        return false;
    }

    // enable window floating mode
    snprintf(cmd, sizeof(cmd), "for_window [pid=%d] floating enable", pid);
    response = sway_request(fd, sway_run_command, cmd);
    if (!response) {
        goto done;
    }
    json_object_put(response);

    // set window position
    snprintf(cmd, sizeof(cmd),
             "for_window [pid=%d] move absolute position %zd %zd", pid, wnd->x,
             wnd->y);
    response = sway_request(fd, sway_run_command, cmd);
    if (!response) {
        goto done;
    }
    json_object_put(response);

    rc = true;

done:
    close(fd);
    return rc;
}

bool compositor_get_focus(struct wndrect* wnd)
{
    return sway_get_focus(wnd);
}

bool compositor_overlay(const struct wndrect* wnd, char** app_id)
{
    (void)app_id;
    return sway_overlay(wnd);
}
