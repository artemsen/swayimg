// SPDX-License-Identifier: MIT
// Image instance: pixel data and meta info.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "types.h"

/** Image context. */
struct image {
    size_t width;     ///< Image width (px)
    size_t height;    ///< Image height (px)
    argb_t* data;     ///< Pointer to pixel data
    bool alpha;       ///< Has alpha channel?
    const char* path; ///< Path to the file
    const char* info; ///< Image meta info
};

/**
 * Load image from file.
 * @param file path to the file to load
 * @return image context or NULL on errors
 */
struct image* image_from_file(const char* file);

/**
 * Load image from stdin data.
 * @return image context or NULL on errors
 */
struct image* image_from_stdin(void);

/**
 * Free image.
 * @param ctx image context to free
 */
void image_free(struct image* ctx);

/**
 * Get image file name.
 * @param ctx image context to free
 * @return file name without path
 */
const char* image_file_name(const struct image* ctx);

/**
 * Flip image vertically.
 * @param ctx image context
 */
void image_flip_vertical(struct image* ctx);

/**
 * Flip image horizontally.
 * @param ctx image context
 */
void image_flip_horizontal(struct image* ctx);

/**
 * Rotate image.
 * @param ctx image context
 * @param angle rotation angle (only 90, 180, or 270)
 */
void image_rotate(struct image* ctx, size_t angle);

/**
 * Add meta info property.
 * @param ctx image context
 * @param key property name
 * @param fmt value format
 */
void image_add_meta(struct image* ctx, const char* key, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));
