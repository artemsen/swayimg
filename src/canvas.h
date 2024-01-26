// SPDX-License-Identifier: MIT
// Canvas used to render images and text to window buffer.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "info.h"
#include "types.h"

// Configuration parameters
#define CANVAS_CFG_ANTIALIASING "antialiasing"
#define CANVAS_CFG_SCALE        "scale"
#define CANVAS_CFG_TRANSPARENCY "transparency"
#define CANVAS_CFG_BACKGROUND   "background"

/**
 * Initialize canvas context.
 */
void canvas_init(void);

/**
 * Reset window parameters.
 * @param width,height new window size
 * @param scale window scale factor
 * @return true if it was the first resize
 */
bool canvas_reset_window(size_t width, size_t height, size_t scale);

/**
 * Reset image position, size and scale.
 * @param width,height new image size
 * @param sc scale to use
 */
void canvas_reset_image(size_t width, size_t height);

/**
 * Recalculate position after rotating image on 90 degree.
 */
void canvas_swap_image_size(void);

/**
 * Draw image on canvas.
 * @param alpha flag to use alpha blending
 * @param img buffer with image data
 * @param wnd window buffer
 */
void canvas_draw(bool aplha, const argb_t* img, argb_t* wnd);

/**
 * Print information text block.
 * @param line array of lines to print
 * @param num total number of lines
 * @param pos block position
 * @param wnd window buffer
 */
void canvas_print(const struct info_line* lines, size_t lines_num,
                  enum info_position pos, argb_t* wnd);

/**
 * Print text block at the center of window.
 * @param line array of lines to print
 * @param num total number of lines
 * @param wnd window buffer
 */
void canvas_print_center(const wchar_t** lines, size_t lines_num, argb_t* wnd);

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
