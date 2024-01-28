// SPDX-License-Identifier: MIT
// Pixel map.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "types.h"

/** Pixel map. */
struct pixmap {
    size_t width;  ///< Width (px)
    size_t height; ///< Height (px)
    argb_t* data;  ///< Pixel data
};

// Attach buffer to pixel map
#define PIXMAP_ATTACH(d, w, h)             \
    {                                      \
        .width = w, .height = h, .data = d \
    }

/**
 * Create new pixel map.
 * @param pm pixmap context to create
 * @param width,height pixmap size
 * @return true if it was the first resize
 */
bool pixmap_create(struct pixmap* pm, size_t width, size_t height);

/**
 * Free pixel map created with `pixmap_create`.
 * @param pm pixmap context to free
 */
void pixmap_free(struct pixmap* pm);

/**
 * Put one pixmap on another.
 * @param dst destination pixmap
 * @param dst_x,dst_y destination left top point
 * @param src source pixmap
 * @param src_x,src_y left top point of source pixmap
 * @param src_scale scale of source pixmap
 * @param alpha flag to use alpha blending
 * @param antialiasing flag to use antialiasing
 */
void pixmap_put(struct pixmap* dst, ssize_t dst_x, ssize_t dst_y,
                const struct pixmap* src, ssize_t src_x, ssize_t src_y,
                float src_scale, bool alpha, bool antialiasing);

/**
 * Fill pixmap with specified color.
 * @param pm pixmap context
 * @param x,y start coordinates, left top point
 * @param width,height region size
 * @param color color to set
 */
void pixmap_fill(struct pixmap* pm, size_t x, size_t y, size_t width,
                 size_t height, argb_t color);

/**
 * Fill pixmap with grid.
 * @param pm pixmap context
 * @param x,y start coordinates, left top point
 * @param width,height region size
 * @param tail_sz size of a single tail
 * @param color0 first grid color
 * @param color1 second grid color
 */
void pixmap_grid(struct pixmap* pm, size_t x, size_t y, size_t width,
                 size_t height, size_t tail_sz, argb_t color0, argb_t color1);

/**
 * Flip pixel map vertically.
 * @param pm pixmap context
 */
void pixmap_flip_vertical(struct pixmap* pm);

/**
 * Flip pixel map horizontally.
 * @param pm pixmap context
 */
void pixmap_flip_horizontal(struct pixmap* pm);

/**
 * Rotate pixel map.
 * @param pm pixmap context
 * @param angle rotation angle (only 90, 180, or 270)
 */
void pixmap_rotate(struct pixmap* pm, size_t angle);
