// SPDX-License-Identifier: MIT
// Canvas used to render images and text to window buffer.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "config.h"
#include "types.h"

/** Canvas context. */
struct canvas;

/** Corner position. */
enum canvas_corner {
    cc_top_left,
    cc_top_right,
    cc_bottom_left,
    cc_bottom_right
};

/** Viewport movement. */
enum canvas_move {
    cm_center,     ///< Center of the image
    cm_cnt_hor,    ///< Center horizontally
    cm_cnt_vert,   ///< Center vertically
    cm_step_left,  ///< One step to the left
    cm_step_right, ///< One step to the right
    cm_step_up,    ///< One step up
    cm_step_down   ///< One step down
};

/** Scaling operations. */
enum canvas_scale {
    cs_fit_or100,  ///< Fit to window, but not more than 100%
    cs_fit_window, ///< Fit to window size
    cs_real_size,  ///< Real image size (100%)
    cs_zoom_in,    ///< Enlarge by one step
    cs_zoom_out    ///< Reduce by one step
};

/**
 * Initialize canvas.
 * @param ctx canvas context
 * @param cfg configuration instance
 */
struct canvas* canvas_init(struct config* cfg);

/**
 * Free canvas resources.
 * @param ctx canvas context
 */
void canvas_free(struct canvas* ctx);

/**
 * Resize canvas window.
 * @param ctx canvas context
 * @param width,height new window size
 * @return true if it was the first resize
 */
bool canvas_resize_window(struct canvas* ctx, size_t width, size_t height);

/**
 * Reset image position, size and scale.
 * @param ctx canvas context
 * @param width,height new image size
 * @param sc scale to use
 */
void canvas_reset_image(struct canvas* ctx, size_t width, size_t height,
                        enum canvas_scale sc);

/**
 * Recalculate position after rotating image on 90 degree.
 * @param ctx canvas context
 */
void canvas_swap_image_size(struct canvas* ctx);

/**
 * Clear canvas window.
 * @param ctx canvas context
 * @param wnd window buffer
 * @param color background color
 */
void canvas_clear(struct canvas* ctx, argb_t* wnd, argb_t color);

/**
 * Draw background grid for transparent images.
 * @param ctx canvas context
 * @param wnd window buffer
 */
void canvas_draw_grid(const struct canvas* ctx, argb_t* wnd);

/**
 * Draw image on canvas.
 * @param ctx canvas context
 * @param alpha flag to use alpha blending
 * @param img buffer with image data
 * @param wnd window buffer
 */
void canvas_draw_image(const struct canvas* ctx, bool aplha, const argb_t* img,
                       argb_t* wnd);

/**
 * Print text on canvas.
 * @param ctx canvas context
 * @param wnd window buffer
 * @param corner text block position
 * @param text printed text
 */
void canvas_print(const struct canvas* ctx, argb_t* wnd,
                  enum canvas_corner corner, const char* text);

/**
 * Move viewport.
 * @param ctx canvas context
 * @param mv viewport movement direction
 * @return true if coordinates were changed
 */
bool canvas_move(struct canvas* ctx, enum canvas_move mv);

/**
 * Scale image on the canvas.
 * @param ctx canvas context
 * @param sc scale operation
 */
void canvas_set_scale(struct canvas* ctx, enum canvas_scale sc);

/**
 * Get current scale.
 * @param ctx canvas context
 * @return current scale, 1.0 = 100%
 */
float canvas_get_scale(struct canvas* ctx);
