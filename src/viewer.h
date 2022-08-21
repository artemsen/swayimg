// SPDX-License-Identifier: MIT
// Business logic of application.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "config.h"
#include "imagelist.h"

/**
 * Start viewer.
 * @param cfg configuration instance
 * @param list list of images to view
 * @return true if operation completed successfully
 */
bool run_viewer(struct config* cfg, struct image_list* list);
