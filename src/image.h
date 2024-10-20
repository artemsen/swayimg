// SPDX-License-Identifier: MIT
// Image instance: pixel data and meta info.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "pixmap.h"

struct image_frame;
struct image_info;

/** Image context. */
struct image {
    size_t index;               ///< Index of the entry in the image list
    char* source;               ///< Image source (e.g. path to the image file)
    const char* name;           ///< Name of the image file
    size_t file_size;           ///< Size of image file
    char* format;               ///< Format description
    struct image_frame* frames; ///< Image frames
    size_t num_frames;          ///< Total number of frames
    bool alpha;                 ///< Image has alpha channel
    struct image_info* info;    ///< Image meta info
    size_t num_info;            ///< Total number of meta info entries
};

/** Image frame. */
struct image_frame {
    struct pixmap pm; ///< Frame data
    size_t duration;  ///< Frame duration in milliseconds (animation)
};

/** Image meta info. */
struct image_info {
    const char* key; ///< Meta key name
    char* value;     ///< Meta value
};

/**
 * Create empty image instance.
 * @return image context or NULL on errors
 */
struct image* image_create(void);

/**
 * Free image.
 * @param ctx image context to free
 */
void image_free(struct image* ctx);

/**
 * Get image file name without path.
 * @param ctx image context
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
 * Create thumbnail from full size image.
 * @param image original image
 * @param thumbnail pixmap with thumbnail of the image
 */
void image_thumbnail(struct image* image, struct pixmap *thumbnail);

/**
 * Set image format description.
 * @param ctx image context
 * @param fmt format description
 */
void image_set_format(struct image* ctx, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

/**
 * Add meta info property.
 * @param ctx image context
 * @param key property name (must be static)
 * @param fmt value format
 */
void image_add_meta(struct image* ctx, const char* key, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));

/**
 * Create single frame, allocate buffer and add frame to the image.
 * @param width frame width in px
 * @param height frame height in px
 * @return pointer to the pixmap associated with the frame, or NULL on errors
 */
struct pixmap* image_allocate_frame(struct image* ctx, size_t width,
                                    size_t height);

/**
 * Create list of empty frames.
 * @param ctx image context
 * @param num total number of frames
 * @return pointer to the frame list, NULL on errors
 */
struct image_frame* image_create_frames(struct image* ctx, size_t num);

/**
 * Free image frames.
 * @param ctx image context
 */
void image_free_frames(struct image* ctx);
