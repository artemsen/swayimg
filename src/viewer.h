// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "config.h"

#include <stddef.h>

/**
 * Start viewer.
 * @param[in] cfg current configuration instance
 * @param[in] files list of files to view
 * @param[in] total total number of files in the list
 * @return true if operation completed successfully
 */
bool run_viewer(config_t* cfg, const char** files, size_t total);
