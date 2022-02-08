// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stdbool.h>
#include <stddef.h>

/**
 * Start viewer.
 * @param[in] files list of files to view
 * @param[in] total total number of files in the list
 * @return true if operation completed successfully
 */
bool run_viewer(const char** files, size_t total);
