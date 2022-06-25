// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "config.h"
#include "filelist.h"

#include <stddef.h>

/**
 * Start viewer.
 * @param[in] cfg current configuration instance
 * @param[in] files list of files to view
 * @return true if operation completed successfully
 */
bool run_viewer(config_t* cfg, file_list_t* files);
