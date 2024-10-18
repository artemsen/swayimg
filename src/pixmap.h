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

// shifts for each channel in argb_t
#define ARGB_A_SHIFT 24
#define ARGB_R_SHIFT 16
#define ARGB_G_SHIFT 8
#define ARGB_B_SHIFT 0

// get channel value from argb_t
#define ARGB_GET_A(c) (((c) >> ARGB_A_SHIFT) & 0xff)
#define ARGB_GET_R(c) (((c) >> ARGB_R_SHIFT) & 0xff)
#define ARGB_GET_G(c) (((c) >> ARGB_G_SHIFT) & 0xff)
#define ARGB_GET_B(c) (((c) >> ARGB_B_SHIFT) & 0xff)

// create argb_t from channel value
#define ARGB_SET_A(a) (((argb_t)(a) & 0xff) << ARGB_A_SHIFT)
#define ARGB_SET_R(r) (((argb_t)(r) & 0xff) << ARGB_R_SHIFT)
#define ARGB_SET_G(g) (((argb_t)(g) & 0xff) << ARGB_G_SHIFT)
#define ARGB_SET_B(b) (((argb_t)(b) & 0xff) << ARGB_B_SHIFT)
#define ARGB(a, r, g, b) \
    (ARGB_SET_A(a) | ARGB_SET_R(r) | ARGB_SET_G(g) | ARGB_SET_B(b))

// convert ABGR to ARGB
#define ABGR_TO_ARGB(c) \
    ((c & 0xff00ff00) | ARGB_SET_R(ARGB_GET_B(c)) | ARGB_SET_B(ARGB_GET_R(c)))

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

/** Pixel map. */
struct pixmap {
    size_t width;  ///< Width (px)
    size_t height; ///< Height (px)
    argb_t* data;  ///< Pixel data
};

/** Scale filters. */
enum pixmap_scale {
    pixmap_nearest, ///< Nearest filter, poor quality but fast
    pixmap_bicubic, ///< Bicubic filter, good quality but slow
    pixmap_average, ///< Average color, downsampling image
};

/**
 * Allocate/reallocate pixel map.
 * @param pm pixmap context to create
 * @param width,height pixmap size
 * @return true pixmap was allocated
 */
bool pixmap_create(struct pixmap* pm, size_t width, size_t height);

/**
 * Free pixel map created with `pixmap_create`.
 * @param pm pixmap context to free
 */
void pixmap_free(struct pixmap* pm);

/**
 * Allocate/reallocate pixel map.
 * @param pm pixmap context
 * @param path path to save to
 * @return true pixmap was saved
 */
bool pixmap_save(struct pixmap* pm, const char* path);

/**
 * Allocate/reallocate pixel map.
 * @param pm pixmap context
 * @param path path to load from
 * @return true pixmap was loaded
 */
bool pixmap_load(struct pixmap* pm, const char* path);

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
 * @param color color to set
 */
void pixmap_blend(struct pixmap* pm, ssize_t x, ssize_t y, size_t width,
                  size_t height, argb_t color);

/**
 * Draw horizontal line.
 * @param pm pixmap context
 * @param x,y start coordinates, left top point
 * @param width line size
 * @param color color to use
 */
void pixmap_hline(struct pixmap* pm, ssize_t x, ssize_t y, size_t width,
                  argb_t color);

/**
 * Draw vertical line.
 * @param pm pixmap context
 * @param x,y start coordinates, left top point
 * @param height line size
 * @param color color to use
 */
void pixmap_vline(struct pixmap* pm, ssize_t x, ssize_t y, size_t height,
                  argb_t color);

/**
 * Draw rectangle with 1px lines.
 * @param pm pixmap context
 * @param x,y start coordinates, left top point
 * @param width,height rectangle size
 * @param color color to use
 */
void pixmap_rect(struct pixmap* pm, ssize_t x, ssize_t y, size_t width,
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
 * @param alpha flag to use alpha blending
 */
void pixmap_copy(const struct pixmap* src, struct pixmap* dst, ssize_t x,
                 ssize_t y, bool alpha);

/**
 * Draw scaled pixmap.
 * @param scaler scale filter to use
 * @param src source pixmap
 * @param dst destination pixmap
 * @param x,y destination left top coordinates
 * @param scale scale of source pixmap
 * @param alpha flag to use alpha blending
 */
void pixmap_scale(enum pixmap_scale scaler, const struct pixmap* src,
                  struct pixmap* dst, ssize_t x, ssize_t y, float scale,
                  bool alpha);
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
