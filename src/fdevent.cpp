// SPDX-License-Identifier: MIT
// Events based on file descriptor.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "fdevent.hpp"

#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <cerrno>

Fd::~Fd()
{
    if (fd != -1) {
        close(fd);
    }
}

FdEvent::FdEvent()
{
    fd = eventfd(0, 0);
}

void FdEvent::set()
{
    const uint64_t value = 1;
    ssize_t len;
    do {
        len = write(fd, &value, sizeof(value));
    } while (len == -1 && errno == EINTR);
}

void FdEvent::reset()
{
    uint64_t value;
    ssize_t len;
    do {
        len = read(fd, &value, sizeof(value));
    } while (len == -1 && errno == EINTR);
}

FdTimer::FdTimer()
{
    fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
}

void FdTimer::reset(const size_t delay, const size_t interval) const
{
    itimerspec ts;
    ts.it_value.tv_sec = delay / 1000;
    ts.it_value.tv_nsec = (delay % 1000) * 1000000;
    ts.it_interval.tv_sec = interval / 1000;
    ts.it_interval.tv_nsec = (interval % 1000) * 1000000;
    timerfd_settime(fd, 0, &ts, nullptr);
}

size_t FdTimer::remain(int fd) const
{
    size_t ms = 0;
    itimerspec ts;

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
