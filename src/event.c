// SPDX-License-Identifier: MIT
// Events processed by the viewer and gallery.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "event.h"

#include <errno.h>
#include <sys/eventfd.h>
#include <unistd.h>

int notification_create(void)
{
    return eventfd(0, 0);
}

void notification_free(int fd)
{
    close(fd);
}

void notification_raise(int fd)
{
    const uint64_t value = 1;
    ssize_t len;

    do {
        len = write(fd, &value, sizeof(value));
    } while (len == -1 && errno == EINTR);
}

void notification_reset(int fd)
{
    uint64_t value;
    ssize_t len;

    do {
        len = read(fd, &value, sizeof(value));
    } while (len == -1 && errno == EINTR);
}
