// SPDX-License-Identifier: MIT
// File descriptor poller.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "fdpoll.h"

#include "array.h"

#include <errno.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

/** File event handler. */
struct watcher {
    fdpoll_handler fn;
    void* data;
};

/** Event poller context */
struct fdpoll {
    struct array* pollfds;  ///< Array of `struct pollfd`
    struct array* watchers; ///< Array of callbacks
};

/** Global application context. */
static struct fdpoll ctx;

void fdpoll_destroy(void)
{
    for (size_t i = 0; i < ctx.pollfds->size; ++i) {
        struct pollfd* pfd = arr_nth(ctx.pollfds, i);
        close(pfd->fd);
    }
    arr_free(ctx.pollfds);
    arr_free(ctx.watchers);
}

void fdpoll_add(int fd, fdpoll_handler cb, void* data)
{
    struct pollfd* pollfd;
    struct array* pollfds;
    struct watcher* watcher;
    struct array* watchers;

    // create/append new entry
    if (ctx.pollfds) {
        pollfds = arr_append(ctx.pollfds, NULL, 1);
        watchers = arr_append(ctx.watchers, NULL, 1);
    } else {
        pollfds = arr_create(1, sizeof(struct pollfd));
        watchers = arr_create(1, sizeof(struct watcher));
    }
    if (!pollfds || !watchers) {
        if (pollfds) {
            ctx.pollfds = arr_resize(pollfds, pollfds->size - 1);
        }
        if (watchers) {
            ctx.watchers = arr_resize(watchers, watchers->size - 1);
        }
        return;
    }
    ctx.pollfds = pollfds;
    ctx.watchers = watchers;

    // get and fill last entry
    pollfd = arr_nth(ctx.pollfds, ctx.pollfds->size - 1);
    pollfd->fd = fd;
    pollfd->events = POLLIN;
    watcher = arr_nth(ctx.watchers, ctx.watchers->size - 1);
    watcher->fn = cb;
    watcher->data = data;
}

int fdpoll_next(void)
{
    struct pollfd* pfd = arr_nth(ctx.pollfds, 0);

    // poll events
    if (poll(pfd, ctx.pollfds->size, -1) < 0) {
        return errno == EINTR ? 0 : errno;
    }

    // call handlers for each active event
    for (size_t i = 0; i < ctx.pollfds->size; ++i) {
        pfd = arr_nth(ctx.pollfds, i);
        if (pfd->revents & POLLIN) {
            struct watcher* watcher = arr_nth(ctx.watchers, i);
            watcher->fn(watcher->data);
        }
    }

    return 0;
}

int fdtimer_add(fdpoll_handler cb, void* data)
{
    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (fd == -1) {
        fd = -errno;
    } else {
        fdpoll_add(fd, cb, data);
    }
    return fd;
}

void fdtimer_reset(int fd, size_t delay, size_t interval)
{
    const struct itimerspec ts = {
        .it_value.tv_sec = delay / 1000,
        .it_value.tv_nsec = (delay % 1000) * 1000000,
        .it_interval.tv_sec = interval / 1000,
        .it_interval.tv_nsec = (interval % 1000) * 1000000,
    };
    timerfd_settime(fd, 0, &ts, NULL);
}

size_t fdtimer_get(int fd)
{
    size_t ms = 0;
    struct itimerspec ts;

    if (timerfd_gettime(fd, &ts) == 0) {
        if (ts.it_value.tv_sec || ts.it_value.tv_nsec) {
            ms = ts.it_value.tv_sec * 1000 + ts.it_value.tv_nsec / 1000000;
        } else if (ts.it_interval.tv_sec || ts.it_interval.tv_nsec) {
            ms =
                ts.it_interval.tv_sec * 1000 + ts.it_interval.tv_nsec / 1000000;
        }
    }

    return ms;
}

int fdevent_add(fdpoll_handler cb, void* data)
{
    int fd = eventfd(0, 0);
    if (fd == -1) {
        fd = -errno;
    } else {
        fdpoll_add(fd, cb, data);
    }
    return fd;
}

void fdevent_set(int fd)
{
    const uint64_t value = 1;
    ssize_t len;
    do {
        len = write(fd, &value, sizeof(value));
    } while (len == -1 && errno == EINTR);
}

void fdevent_reset(int fd)
{
    uint64_t value;
    ssize_t len;
    do {
        len = read(fd, &value, sizeof(value));
    } while (len == -1 && errno == EINTR);
}
