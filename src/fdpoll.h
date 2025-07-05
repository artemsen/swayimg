// SPDX-License-Identifier: MIT
// File descriptor poller.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stddef.h>

/**
 * Event handler.
 * @param data user data
 */
typedef void (*fdpoll_handler)(void* data);

/**
 * Initialize global poller context.
 */
void fdpoll_init(void);

/**
 * Destroy global poller context.
 */
void fdpoll_destroy(void);

/**
 * Add file descriptor for polling.
 * @param fd file descriptor for polling
 * @param cb callback function to handle event
 * @param data user defined data to pass to callback
 */
void fdpoll_add(int fd, fdpoll_handler cb, void* data);

/**
 * Wait and handle next event.
 * @return 0 on success or error code
 */
int fdpoll_next(void);

/**
 * Add timer for polling.
 * @param cb callback function to handle event
 * @param data user defined data to pass to callback
 * @return timer file descriptor or negative number with error code
 */
int fdtimer_add(fdpoll_handler cb, void* data);

/**
 * Restart timer.
 * @param fd timer file descriptor
 * @param delay time before timer trigger (ms)
 * @param interval time for periodic trigger (ms)
 */
void fdtimer_reset(int fd, size_t delay, size_t interval);

/**
 * Get remaining time.
 * @param fd timer file descriptor
 * @return remaining time in milliseconds, 0 if stopped
 */
size_t fdtimer_get(int fd);

/**
 * Add event for polling.
 * @param cb callback function to handle event
 * @param data user defined data to pass to callback
 * @return timer file descriptor or negative number with error code
 */
int fdevent_add(fdpoll_handler cb, void* data);

/**
 * Set event.
 * @param fd event file descriptor
 */
void fdevent_set(int fd);

/**
 * Reset event.
 * @param fd event file descriptor
 */
void fdevent_reset(int fd);
