// SPDX-License-Identifier: MIT
// Thread pool.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stddef.h>

/**
 * Function to handle a task.
 * @param data pointer to task-specific data
 */
typedef void (*tpool_worker)(void* data);

/**
 * Function to free task-specific data.
 * @param data pointer to task-specific data
 */
typedef void (*tpool_free)(void* data);

/**
 * Initialize global thread pool context.
 */
void tpool_init(void);

/**
 * Destroy global thread pool context.
 */
void tpool_destroy(void);

/**
 * Get number of threads in the pool/
 * @return size of the thread pool
 */
size_t tpool_threads(void);

/**
 * Add task to execution queue.
 * @param wfn work function to handle a task
 * @param ffn function to free task-specific data
 * @param data pointer to task-specific data
 */
void tpool_add_task(tpool_worker wfn, tpool_free ffn, void* data);

/**
 * Cancel all pending tasks (clear execution queue).
 */
void tpool_cancel(void);

/**
 * Wait until all tasks are completed.
 */
void tpool_wait(void);
