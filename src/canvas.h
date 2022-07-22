// SPDX-License-Identifier: MIT
// Canvas used to render images and text to window buffer.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "config.h"
#include "image.h"
#include "window.h"

/** Canvas context. */
struct canvas {
    double scale;        ///< Scale, 1.0 = 100%
    size_t img_w;        ///< Image width
    size_t img_h;        ///< Image height
    ssize_t img_x;       ///< Horizontal offset of the image on canvas
    ssize_t img_y;       ///< Vertical offset of the image on canvas
    size_t wnd_w;        ///< Window width
    size_t wnd_h;        ///< Window height
    rotate_t rotate;     ///< Rotation angle
    flip_t flip;         ///< Flip mode flags
    void* font_handle;   ///< Font handle to draw text
    uint32_t font_color; ///< Color used for text
};

/**
 * Initialize canvas.
 * @param[in] ctx canvas context
 * @param[in] width window width
 * @param[in] height window height
 * @param[in] cfg configuration instance
 */
void init_canvas(struct canvas* ctx, size_t width, size_t height,
                 config_t* cfg);

/**
 * Free canvas resources.
 * @param[in] ctx canvas context
 */
void free_canvas(struct canvas* ctx);

/**
 * Attach image to canvas and reset canvas parameters to default values.
 * @param[in] ctx canvas context
 * @param[in] img displayed image
 */
void attach_image(struct canvas* ctx, const image_t* img);

/**
 * Attach window to canvas and clear the it.
 * @param[in] ctx canvas context
 * @param[in] wnd target window description
 * @param[in] color background color
 */
void attach_window(struct canvas* ctx, struct window* wnd, uint32_t color);

/**
 * Draw image.
 * @param[in] ctx canvas context
 * @param[in] img image to draw
 * @param[in] wnd target window description
 */
void draw_image(const struct canvas* ctx, const image_t* img,
                struct window* wnd);

/**
 * Draw background grid for transparent images.
 * @param[in] ctx canvas context
 * @param[in] wnd target window description
 */
void draw_grid(const struct canvas* ctx, struct window* wnd);

/**
 * Print text.
 * @param[in] ctx canvas context
 * @param[in] wnd target window description
 * @param[in] pos text block position
 * @param[in] text text to output
 */
void print_text(const struct canvas* ctx, struct window* wnd,
                text_position_t pos, const char* text);

/**
 * Move view point.
 * @param[in] ctx canvas context
 * @param[in] direction viewport movement direction
 * @return true if coordinates were changed
 */
bool move_viewpoint(struct canvas* ctx, move_t direction);

/**
 * Apply scaling operation.
 * @param[in] ctx canvas context
 * @param[in] op scale operation type
 * @return true if scale was changed
 */
bool apply_scale(struct canvas* ctx, scale_t op);

/**
 * Rotate image on 90 degrees.
 * @param[in] ctx canvas context
 * @param[in] clockwise rotate direction
 */
void apply_rotate(struct canvas* ctx, bool clockwise);

/**
 * Flip image.
 * @param[in] ctx canvas context
 * @param[in] flip axis type
 */
void apply_flip(struct canvas* ctx, flip_t flip);
