// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "canvas.h"
#include "image.h"
#include "sway.h"

#include <stddef.h>

#define COLOR_MAX       0xffffff
#define BACKGROUND_GRID UINT32_MAX

/** Viewer context. */
struct viewer_t {
    struct {
        const char** files; ///< List of files to view
        size_t total;       ///< Total number of files in the list
        size_t current;     ///< Index of currently displayed image in the list
    } file_list;

    scale_t scale;   ///< Initial scale by default
    struct rect wnd; ///< Window geometry
    uint32_t bkg;    ///< Background color
    bool fullscreen; ///< Full screen mode
    bool show_info;  ///< Show image info

    image_t* image;  ///< Currently displayed image
    canvas_t canvas; ///< Canvas context
};

extern struct viewer_t viewer;

/**
 * Start viewer.
 * @return true if operation completed successfully
 */
bool run_viewer(void);
