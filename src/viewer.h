// SPDX-License-Identifier: MIT
// Business logic of application.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "config.h"
#include "filelist.h"

#include <stddef.h>

/**
 * Start viewer.
 * @param cfg current configuration instance
 * @param files list of files to view
 * @return true if operation completed successfully
 */
bool run_viewer(struct config* cfg, struct file_list* files);
