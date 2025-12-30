// SPDX-License-Identifier: MIT
// Pixel map.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/** ARGB color. */
typedef uint32_t argb_t;

// max component value
#define ARGB_MAX_COLOR 0xff

// shifts for each channel in argb_t
#define ARGB_A_SHIFT 24
#define ARGB_R_SHIFT 16
#define ARGB_G_SHIFT 8
#define ARGB_B_SHIFT 0

// get channel value from argb_t
#define ARGB_GET_A(c) (((c) >> ARGB_A_SHIFT) & ARGB_MAX_COLOR)
#define ARGB_GET_R(c) (((c) >> ARGB_R_SHIFT) & ARGB_MAX_COLOR)
#define ARGB_GET_G(c) (((c) >> ARGB_G_SHIFT) & ARGB_MAX_COLOR)
#define ARGB_GET_B(c) (((c) >> ARGB_B_SHIFT) & ARGB_MAX_COLOR)

// create argb_t from channel value
#define ARGB_SET_A(a) (((argb_t)(a) & ARGB_MAX_COLOR) << ARGB_A_SHIFT)
#define ARGB_SET_R(r) (((argb_t)(r) & ARGB_MAX_COLOR) << ARGB_R_SHIFT)
#define ARGB_SET_G(g) (((argb_t)(g) & ARGB_MAX_COLOR) << ARGB_G_SHIFT)
#define ARGB_SET_B(b) (((argb_t)(b) & ARGB_MAX_COLOR) << ARGB_B_SHIFT)
#define ARGB(a, r, g, b) \
    (ARGB_SET_A(a) | ARGB_SET_R(r) | ARGB_SET_G(g) | ARGB_SET_B(b))

// convert ABGR to ARGB
#define ABGR_TO_ARGB(c) \
    ((c & 0xff00ff00) | ARGB_SET_R(ARGB_GET_B(c)) | ARGB_SET_B(ARGB_GET_R(c)))

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

enum pixmap_format {
    pixmap_argb, // with alpha channel
    pixmap_xrgb, // without alpha channel
};

/** Pixel map. */
struct pixmap {
    enum pixmap_format format; ///< Format
    size_t width;              ///< Width (px)
    size_t height;             ///< Height (px)
    argb_t* data;              ///< Pixel data
};

/**
 * Allocate/reallocate pixmap.
 * @param pm pixmap context to create
 * @param format pixmap format
 * @param width,height pixmap size
 * @return true pixmap was allocated
 */
bool pixmap_create(struct pixmap* pm, enum pixmap_format format, size_t width,
                   size_t height);

/**
 * Free pixmap created with `pixmap_create`.
 * @param pm pixmap context to free
 */
void pixmap_free(struct pixmap* pm);

/**
 * Fill area with specified color.
 * @param pm pixmap context
 * @param x,y start coordinates, left top point
 * @param width,height region size
 * @param color color to set
 */
void pixmap_fill(struct pixmap* pm, ssize_t x, ssize_t y, size_t width,
                 size_t height, argb_t color);

/**
 * Fill whole pixmap except specified area.
 * @param pm pixmap context
 * @param x,y top left corner of excluded area
 * @param width,height excluded area size
 * @param color color to set
 */
void pixmap_inverse_fill(struct pixmap* pm, ssize_t x, ssize_t y, size_t width,
                         size_t height, argb_t color);

/**
 * Blend area with specified color.
 * @param pm pixmap context
 * @param x,y start coordinates, left top point
 * @param width,height region size
 * @param color color to blend
 */
void pixmap_blend(struct pixmap* pm, ssize_t x, ssize_t y, size_t width,
                  size_t height, argb_t color);

/**
 * Draw horizontal line.
 * @param pm pixmap context
 * @param x,y start coordinates, left top point
 * @param width line size
 * @param thickness line thickness, growing in @ref y direction
 * @param color color to use
 */
void pixmap_hline(struct pixmap* pm, ssize_t x, ssize_t y, size_t width,
                  size_t thickness, argb_t color);

/**
 * Draw vertical line.
 * @param pm pixmap context
 * @param x,y start coordinates, left top point
 * @param height line size
 * @param thickness line thickness, growing in @ref x direction
 * @param color color to use
 */
void pixmap_vline(struct pixmap* pm, ssize_t x, ssize_t y, size_t height,
                  size_t thickness, argb_t color);

/**
 * Draw rectangle.
 * @param pm pixmap context
 * @param x,y start coordinates, left top point
 * @param width,height rectangle size
 * @param thickness rectangle line thickness, growing outwards
 * @param color color to use
 */
void pixmap_rect(struct pixmap* pm, ssize_t x, ssize_t y, size_t width,
                 size_t height, size_t thickness, argb_t color);

/**
 * Fill pixmap with grid.
 * @param pm pixmap context
 * @param x,y start coordinates, left top point
 * @param width,height region size
 * @param tail_sz size of a single tail
 * @param color0 first grid color
 * @param color1 second grid color
 */
void pixmap_grid(struct pixmap* pm, ssize_t x, ssize_t y, size_t width,
                 size_t height, size_t tail_sz, argb_t color0, argb_t color1);

/**
 * Apply mask to pixmap: change color according alpha channel.
 * @param pm destination pixmap
 * @param x,y destination left top point
 * @param mask array with alpha channel mask
 * @param width,height mask size
 * @param color color to set
 */
void pixmap_apply_mask(struct pixmap* pm, ssize_t x, ssize_t y,
                       const uint8_t* mask, size_t width, size_t height,
                       argb_t color);

/**
 * Draw one pixmap on another.
 * @param src source pixmap
 * @param dst destination pixmap
 * @param x,y destination left top coordinates
 */
void pixmap_copy(const struct pixmap* src, struct pixmap* dst, ssize_t x,
                 ssize_t y);

/**
 * Flip pixmap vertically.
 * @param pm pixmap context
 */
void pixmap_flip_vertical(struct pixmap* pm);

/**
 * Flip pixmap horizontally.
 * @param pm pixmap context
 */
void pixmap_flip_horizontal(struct pixmap* pm);

/**
 * Rotate pixmap.
 * @param pm pixmap context
 * @param angle rotation angle (only 90, 180, or 270)
 */
void pixmap_rotate(struct pixmap* pm, size_t angle);

/**
 * Extend image to fill entire pixmap (zoom to fill and blur).
 * @param pm pixmap context
 * @param x,y top left coordinates of existing image on pixmap surface
 * @param width,height size of existing image on pixmap surface
 */
void pixmap_bkg_extend(struct pixmap* pm, ssize_t x, ssize_t y, size_t width,
                       size_t height);

/**
 * Extend image to fill entire pixmap (mirror and blur).
 * @param pm pixmap context
 * @param x,y top left coordinates of existing image on pixmap surface
 * @param width,height size of existing image on pixmap surface
 */
void pixmap_bkg_mirror(struct pixmap* pm, ssize_t x, ssize_t y, size_t width,
                       size_t height);

/**
 * Alpha blending.
 * @param src top pixel
 * @param dst bottom pixel
 */
static inline void pixmap_alpha_blend(argb_t src, argb_t* dst)
{
    const uint8_t src_a = ARGB_GET_A(src);

    if (src_a != ARGB_MAX_COLOR) {
        const uint8_t inv_a = ARGB_MAX_COLOR - src_a;
        const uint8_t dst_a = ARGB_GET_A(*dst);
        src = ARGB(max(src_a, dst_a),
                   (src_a * ARGB_GET_R(src) + inv_a * ARGB_GET_R(*dst)) /
                       ARGB_MAX_COLOR,
                   (src_a * ARGB_GET_G(src) + inv_a * ARGB_GET_G(*dst)) /
                       ARGB_MAX_COLOR,
                   (src_a * ARGB_GET_B(src) + inv_a * ARGB_GET_B(*dst)) /
                       ARGB_MAX_COLOR);
    }

    *dst = src;
}
