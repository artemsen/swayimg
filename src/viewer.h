// SPDX-License-Identifier: MIT
// Business logic of application and UI event handlers.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "event.h"
#include "image.h"

// Configuration parameters
#define VIEWER_CFG_SCALE          "scale"
#define VIEWER_CFG_ANTIALIASING   "antialiasing"
#define VIEWER_CFG_FIXED          "fixed"
#define VIEWER_CFG_TRANSPARENCY   "transparency"
#define VIEWER_CFG_BACKGROUND     "background"
#define VIEWER_CFG_SLIDESHOW      "slideshow"
#define VIEWER_CFG_SLIDESHOW_TIME "slideshow_time"

/**
 * Create global viewer context.
 */
void viewer_create(void);

/**
 * Initialize global viewer context.
 * @param image initial image to open
 * @param index index of the image in the image list
 */
void viewer_init(struct image* image, size_t index);

/**
 * Destroy global viewer context.
 */
void viewer_destroy(void);

/**
 * Event handler, see `event_handler` for details.
 */
void viewer_handle(const struct event* event);
