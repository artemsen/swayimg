// SPDX-License-Identifier: MIT
// File system watcher (inotify).
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stdbool.h>

/** File system event types. */
enum fswatch_evtype {
    fswatch_create,
    fswatch_modify,
    fswatch_remove,
};

/** File system event. */
struct fswatch_event {
    enum fswatch_evtype type; ///< Event type
    bool is_dir;              ///< Event object type (file/dir)
    const char* dir;          ///< Directory path
    const char* name;         ///< File name
};

/**
 * File system event handler.
 * @param event pointer to the event to handle
 */
typedef void (*fswatch_callback)(const struct fswatch_event* event);

/**
 * Initialize global watch context.
 */
void fswatch_init(void);

/**
 * Destroy global watch context.
 */
void fswatch_destroy(void);

/**
 * Set the current file to watch.
 * @param path current file path
 * @param handler event handler
 */
void fswatch_set_file(const char* path, fswatch_callback handler);

/**
 * Add directory to watch.
 * @param path directory path
 * @param handler event handler
 */
void fswatch_add_dir(const char* path, fswatch_callback handler);

/**
 * Remove directory from watched.
 * @param path path to the directory
 */
void fswatch_rm_dir(const char* path);
