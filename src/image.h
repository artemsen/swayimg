// SPDX-License-Identifier: MIT
// Image instance: pixel data and meta info.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "list.h"
#include "pixmap_scale.h"

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

    char* source;      ///< Image source (e.g. path to the image file)
    uint8_t* file_raw; ///< Raw image file data
    size_t file_size;  ///< Size of the image file
    time_t file_time;  ///< File modification time

    size_t index;     ///< Index of the image
    const char* name; ///< Name of the image file
    char* parent_dir; ///< Parent directory name

    char* format;            ///< Format description
    struct image_info* info; ///< Image meta info
    bool alpha;              ///< Image has alpha channel

    struct image_frame* frames; ///< Image frames
    size_t num_frames;          ///< Total number of frames

    struct pixmap thumbnail; ///< Image thumbnail
};

/** Image loading status. */
enum image_status {
    imgload_success,     ///< Image was decoded successfully
    imgload_unsupported, ///< Unsupported format
    imgload_fmterror,    ///< Invalid data format
    imgload_ioerror      ///< IO errors
};

/** Image data types (used as mask for freeing data). */
#define IMGFREE_FRAMES 1
#define IMGFREE_THUMB  2
#define IMGFREE_ALL    7

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
 * Load image from specified source.
 * @param img image context
 * @return loading status
 */
enum image_status image_load(struct image* img);

/**
 * Update image data (move) from another instance.
 * @param img target image instance
 * @param from adopted image instance
 */
void image_update(struct image* img, struct image* from);

/**
 * Free image instance.
 * @param img image context
 * @param dt image data type to free (mask with `IMGFREE_*`)
 */
void image_free(struct image* img, size_t dt);

/**
 * Check if image has frame data.
 * @param img image context
 * @return true if image has frame data
 */
bool image_has_frames(const struct image* img);

/**
 * Check if image has thumbnail.
 * @param img image context
 * @return true if image has thumbnail
 */
bool image_has_thumb(const struct image* img);

/**
 * Flip image vertically.
 * @param img image context
 */
void image_flip_vertical(struct image* img);

/**
 * Flip image horizontally.
 * @param img image context
 */
void image_flip_horizontal(struct image* img);

/**
 * Rotate image.
 * @param img image context
 * @param angle rotation angle (only 90, 180, or 270)
 */
void image_rotate(struct image* img, size_t angle);

/**
 * Create thumbnail.
 * @param img image context
 * @param size thumbnail size in pixels
 * @param fill scale mode (fill/fit)
 * @param aa_mode anti-aliasing mode
 * @return true if thumbnail created
 */
bool image_thumb_create(struct image* img, size_t size, bool fill,
                        enum aa_mode aa_mode);

/**
 * Load thumbnail from specified file.
 * @param img image context
 * @param path path to the thumbnail file to load
 * @return true if thumbnail loaded
 */
bool image_thumb_load(struct image* img, const char* path);

/**
 * Save thumbnail to specified file.
 * @param img image context
 * @param path path to the thumbnail file
 * @return true if thumbnail saved
 */
bool image_thumb_save(const struct image* img, const char* path);
