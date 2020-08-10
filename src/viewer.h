// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "sway.h"
#include <stdbool.h>

/** Viewer parameters. */
struct viewer {
    const char* file;    ///< Path to image file to show
    const char* app_id;  ///< Application ID (NULL=auto)
    int scale;           ///< Image scale (0=auto)
    struct rect* wnd;    ///< Window geometry
};

/**
 * Start viewer.
 * @param[in] params viewer parameters
 * @return true if operation completed successfully
 */
bool show_image(const struct viewer* params);
