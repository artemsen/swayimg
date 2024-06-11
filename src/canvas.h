// SPDX-License-Identifier: MIT
// Canvas used to render images and text to window buffer.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.h"
#include "info.h"

// Configuration parameters
#define CANVAS_CFG_ANTIALIASING "antialiasing"
#define CANVAS_CFG_SCALE        "scale"
#define CANVAS_CFG_TRANSPARENCY "transparency"
#define CANVAS_CFG_BACKGROUND   "background"
#define CANVAS_CFG_FIXED        "fixed"

/**
 * Initialize canvas context.
 */
void canvas_init(void);

/**
 * Reset window parameters.
 */
void canvas_reset_window(void);

/**
 * Reset image position, size and scale.
 * @param width,height new image size
 */
void canvas_reset_image(size_t width, size_t height);

/**
 * Recalculate position after rotating image on 90 degree.
 */
void canvas_swap_image_size(void);

/**
 * Draw image on window.
 * @param wnd destination window
 * @param img image to draw
 * @param frame frame number to draw
 */
void canvas_draw_image(struct pixmap* wnd, const struct image* img,
                       size_t frame);

/**
 * Move viewport.
 * @param horizontal axis along which to move (false for vertical)
 * @param percent percentage increment to current position
 * @return true if coordinates were changed
 */
bool canvas_move(bool horizontal, ssize_t percent);

/**
 * Move viewport.
 * @param dx,dy delta between current and new position
 * @return true if coordinates were changed
 */
bool canvas_drag(int dx, int dy);

/**
 * Zoom image.
 * @param op zoom operation name
 */
void canvas_zoom(const char* op);

/**
 * Get current scale.
 * @return current scale, 1.0 = 100%
 */
float canvas_get_scale(void);

/**
 * Switch antialiasing.
 * @return current state
 */
bool canvas_switch_aa(void);
