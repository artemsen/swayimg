// SPDX-License-Identifier: MIT
// Inter Process Communication: application control via socket.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "ipc.h"

#include "application.h"

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define REQ_MAX_LEN 1024

/** IPC client data. */
struct ipc_client {
    int fd;
};

/** IPC server socket. */
static int socket_fd = -1;

/** IPC client handler: detached thread. */
static void* client_handler(void* data)
{
    struct ipc_client* cln = data;
    char buffer[REQ_MAX_LEN];

    while (true) {
        struct action* actions;
        const ssize_t rc = recv(cln->fd, buffer, sizeof(buffer) - 1, 0);
        if (rc <= 0) {
            break;
        }

        // parse actions from request
        buffer[rc] = 0;
        actions = action_create(buffer);
        if (!actions) {
            fprintf(stderr, "Invalid IPC request: %s\n", buffer);
        }

        // add action one-by-one
        while (actions) {
            struct action* next = actions->next;
            actions->next = NULL;
            app_apply_action(actions, true);
            actions = next;
        }
    }

    close(cln->fd);
    free(cln);

    return NULL;
}

/** IPC server handler: new client connected. */
static void connection_handler(__attribute__((unused)) void* data)
{
    pthread_t tid;
    struct ipc_client* cln;
    int client_fd;

    client_fd = accept(socket_fd, NULL, NULL);
    if (client_fd == -1) {
        return;
    }

    cln = malloc(sizeof(struct ipc_client));
    if (!cln) {
        close(client_fd);
        return;
    }
    cln->fd = client_fd;

    if (pthread_create(&tid, NULL, client_handler, cln) == 0) {
        pthread_detach(tid);
    } else {
        close(client_fd);
        free(cln);
    }
}

bool ipc_start(const char* path)
{
    struct sockaddr_un sa = { .sun_family = AF_UNIX };
    const size_t len = path ? strlen(path) : 0;

    if (len == 0 || len >= sizeof(sa.sun_path)) {
        fprintf(stderr, "Invalid IPC socket path\n");
        return false;
    }

    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        const int rc = errno;
        fprintf(stderr, "Failed to create IPC %s: [%d] %s\n", path, rc,
                strerror(rc));
        goto fail;
    }

    unlink(path);

    memcpy(sa.sun_path, path, len);
    if (bind(socket_fd, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
        const int rc = errno;
        fprintf(stderr, "Failed to bind socket %s: [%d] %s\n", path, rc,
                strerror(rc));
        goto fail;
    }

    if (listen(socket_fd, 1) == -1) {
        const int rc = errno;
        fprintf(stderr, "Failed to listen socket %s: [%d] %s\n", path, rc,
                strerror(rc));
        goto fail;
    }

    app_watch(socket_fd, connection_handler, NULL);

    return true;

fail:
    if (socket_fd != -1) {
        close(socket_fd);
        socket_fd = -1;
    }
    return false;
}

void ipc_stop(void)
{
    if (socket_fd != -1) {
        close(socket_fd);
        socket_fd = -1;
    }
}
