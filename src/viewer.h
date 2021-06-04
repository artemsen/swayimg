// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "sway.h"
#include "browser.h"

#include <stddef.h>
#include <stdbool.h>

/** Viewer parameters. */
struct viewer {
    int scale;        ///< Initial image scale in percent (0=auto)
    struct rect wnd;  ///< Window geometry (NULL=auto)
    bool fullscreen;  ///< Full screen mode
    bool show_info;   ///< Show image info
    browser* browser;
};
extern struct viewer viewer;

/**
 * Start viewer.
 * @return true if operation completed successfully
 */
bool show_image();
