// SPDX-License-Identifier: MIT
// File system operations.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stddef.h>

/** File system event types. */
enum fsevent {
    fsevent_create,
    fsevent_modify,
    fsevent_remove,
};

/**
 * File system event handler.
 * @param event event type
 * @param path absolute path, ends with "/" if it is a directory
 */
typedef void (*fs_monitor_cb)(enum fsevent type, const char* path);

/**
 * Initialize global file system monitor context.
 * @param handler event handler
 */
void fs_monitor_init(fs_monitor_cb handler);

/**
 * Destroy global file system monitor context.
 */
void fs_monitor_destroy(void);

/**
 * Register file or directory in monitor.
 * @param path watched path
 */
void fs_monitor_add(const char* path);

/**
 * Append subdir/file to the path.
 * @param file subdir/file name
 * @param path output buffer
 * @param path_max size of the buffer
 * @return length of the result path excluding the last null
 */
size_t fs_append_path(const char* file, char* path, size_t path_max);

/**
 * Get absolute path from relative.
 * @param relative relative file path
 * @param path output buffer
 * @param path_max size of the buffer
 * @return length of the absolute path excluding the last null
 */
size_t fs_abspath(const char* relative, char* path, size_t path_max);

/**
 * Construct path started with environment variable.
 * @param env_name environment variable (path prefix)
 * @param postfix constant postfix
 * @param path output buffer
 * @param path_max size of the buffer
 * @return length of the result path excluding the last null
 */
size_t fs_envpath(const char* env_name, const char* postfix, char* path,
                  size_t path_max);
