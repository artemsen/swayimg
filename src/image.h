// SPDX-License-Identifier: MIT
// Image instance: pixel data and meta info.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "list.h"
#include "pixmap.h"

// File name used for image, that is read from stdin through pipe
#define LDRSRC_STDIN     "stdin://"
#define LDRSRC_STDIN_LEN (sizeof(LDRSRC_STDIN) - 1)

// Special prefix used to load images from external command output
#define LDRSRC_EXEC     "exec://"
#define LDRSRC_EXEC_LEN (sizeof(LDRSRC_EXEC) - 1)

/** Image frame. */
struct image_frame {
    struct pixmap pm; ///< Frame data
    size_t duration;  ///< Frame duration in milliseconds (animation)
};

/** Image meta info. */
struct image_info {
    struct list list; ///< Links to prev/next entry
    char* key;        ///< Meta key name
    char* value;      ///< Meta value
};

/** Image context. */
struct image {
    struct list list; ///< Links to prev/next entry in the image list

    char* source;     ///< Image source (e.g. path to the image file)
    size_t file_size; ///< Size of image file
    time_t file_time; ///< File modification time

    size_t index;     ///< Index of the image
    const char* name; ///< Name of the image file
    char* parent_dir; ///< Parent directory name

    char* format;               ///< Format description
    struct image_frame* frames; ///< Image frames
    size_t num_frames;          ///< Total number of frames
    struct image_info* info;    ///< Image meta info
    bool alpha;                 ///< Image has alpha channel

    size_t ref_count; ///< Reference count
};

/** Loader status. */
enum loader_status {
    ldr_success,     ///< Image was decoded successfully
    ldr_unsupported, ///< Unsupported format
    ldr_fmterror,    ///< Invalid data format
    ldr_ioerror      ///< IO errors
};

/**
 * Get list of supported image formats.
 * @return list of supported formats
 */
const char* image_formats(void);

/**
 * Create empty image instance.
 * @param source image source
 * @return image context or NULL on errors
 */
struct image* image_create(const char* source);

/**
 * Increment usage counter.
 * @param img image context
 */
void image_addref(struct image* img);

/**
 * Decrement usage counter.
 * @param img image context
 */
void image_deref(struct image* img);

/**
 * Load image from specified source.
 * @param img image context
 * @return loading status
 */
enum loader_status image_load(struct image* img);

/**
 * Unload image: free pixmaps, meta, etc.
 * @param img image context to unload
 */
void image_unload(struct image* img);

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
 * Set image format description.
 * @param ctx image context
 * @param fmt format description
 */
void image_set_format(struct image* ctx, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

/**
 * Add meta info property.
 * @param ctx image context
 * @param key property name
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
