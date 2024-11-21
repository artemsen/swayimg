// SPDX-License-Identifier: MIT
// Business logic of application and UI event handlers.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "config.h"
#include "event.h"
#include "image.h"

// Configuration parameters
#define VIEWER_SECTION        "viewer"
#define VIEWER_WINDOW         "window"
#define VIEWER_TRANSPARENCY   "transparency"
#define VIEWER_SCALE          "scale"
#define VIEWER_POSITION       "position"
#define VIEWER_FIXED          "fixed"
#define VIEWER_ANTIALIASING   "antialiasing"
#define VIEWER_SLIDESHOW      "slideshow"
#define VIEWER_SLIDESHOW_TIME "slideshow_time"
#define VIEWER_HISTORY        "history"
#define VIEWER_PRELOAD        "preload"

/**
 * Initialize global viewer context.
 * @param cfg config instance
 * @param image initial image to open
 */
void viewer_init(struct config* cfg, struct image* image);

/**
 * Destroy global viewer context.
 */
void viewer_destroy(void);

/**
 * Event handler, see `event_handler` for details.
 */
void viewer_handle(const struct event* event);
