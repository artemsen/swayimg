// SPDX-License-Identifier: MIT
// Events based on file descriptor.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <cstddef>

/** Base class for file descriptors. */
class Fd {
public:
    Fd() = default;
    Fd(int d)
        : fd(d) { };

    /** Destructor. */
    virtual ~Fd();

    /** Cast to native file descriptor. */
    operator int() const { return fd; }

    int fd = -1; ///< Event file descriptor
};

/** eventfd wrapper. */
class FdEvent : public Fd {
public:
    /** Constructor: create eventfd file descriptor. */
    FdEvent();

    /** Set event. */
    void set();

    /** Reset event. */
    void reset();
};

/** Timers. */
class FdTimer : public Fd {
public:
    /**
     * Constructor: create timer file descriptor.
     */
    FdTimer();

    /**
     * Restart timer.
     * @param delay time before timer trigger (ms)
     * @param interval time for periodic trigger (ms)
     */
    void reset(const size_t delay, const size_t interval) const;

    /**
     * Get the remaining time.
     * @return remaining time in milliseconds, 0 if stopped
     */
    size_t remain(int fd) const;
};
