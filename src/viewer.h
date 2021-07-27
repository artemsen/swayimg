// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "sway.h"

#include <stddef.h>
#include <stdbool.h>

// Initial scale: reduce the image to fit the window. If the image is smaller
// then the window then use 100% scale.
#define SCALE_REDUCE_OR_100   0
// Initial scale: reduce or enlarge the image to fit the window.
#define SCALE_FIT_TO_WINDOW  -1

/** Viewer parameters. */
struct viewer {
    int scale;        ///< Initial image scale in percent or one of SCALE_* macros
    struct rect wnd;  ///< Window geometry
    bool fullscreen;  ///< Full screen mode
    bool show_info;   ///< Show image info
};
extern struct viewer viewer;

/**
 * Start viewer.
 * @return true if operation completed successfully
 */
bool run_viewer(void);
