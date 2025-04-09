// SPDX-License-Identifier: MIT
// File system operations.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stddef.h>

/**
 * Get absolute path from relative.
 * @param relative relative file path
 * @param path output buffer
 * @param path_max size of the buffer
 * @return length of the absolute path without last null
 */
size_t fs_abspath(const char* relative, char* path, size_t path_max);

/**
 * Construct path started with environment variable.
 * @param env_name environment variable (path prefix)
 * @param postfix constant postfix
 * @param path output buffer
 * @param path_max size of the buffer
 * @return length of the result path without last null
 */
size_t fs_envpath(const char* env_name, const char* postfix, char* path,
                  size_t path_max);
