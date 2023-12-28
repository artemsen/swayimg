// SPDX-License-Identifier: MIT
// Canvas used to render images and text to window buffer.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "info.h"
#include "types.h"

/** Scaling operations. */
enum canvas_scale {
    cs_fit_or100,   ///< Fit to window, but not more than 100%
    cs_fit_window,  ///< Fit to window size
    cs_fill_window, ///< Fill the window
    cs_real_size,   ///< Real image size (100%)
};

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
void canvas_reset_image(size_t width, size_t height, enum canvas_scale sc);

/**
 * Recalculate position after rotating image on 90 degree.
 */
void canvas_swap_image_size(void);

/**
 * Clear canvas window.
 * @param wnd window buffer
 */
void canvas_clear(argb_t* wnd);

/**
 * Draw image on canvas.
 * @param alpha flag to use alpha blending
 * @param img buffer with image data
 * @param wnd window buffer
 */
void canvas_draw_image(bool aplha, const argb_t* img, argb_t* wnd);

/**
 * Print information text block.
 * @param line array of lines to pprint
 * @param num total number of lines
 * @param pos block position
 * @param wnd window buffer
 */
void canvas_print(const struct info_line* lines, size_t lines_num,
                  enum info_position pos, argb_t* wnd);

/**
 * Move viewport.
 * @param horizontal axis along which to move (false for vertical)
 * @param percent percentage increment to current position
 * @return true if coordinates were changed
 */
bool canvas_move(bool horizontal, ssize_t percent);

/**
 * Zoom in/out.
 * @param percent percentage increment to current scale
 */
void canvas_zoom(ssize_t percent);

/**
 * Set fixed scale for the image.
 * @param sc scale to set
 */
void canvas_set_scale(enum canvas_scale sc);

/**
 * Get current scale.
 * @return current scale, 1.0 = 100%
 */
float canvas_get_scale(void);
