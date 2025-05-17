// SPDX-License-Identifier: MIT
// Image instance: pixel data and meta info.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "array.h"
#include "list.h"
#include "render.h"

// File name used for image, that is read from stdin through pipe
#define LDRSRC_STDIN     "stdin://"
#define LDRSRC_STDIN_LEN (sizeof(LDRSRC_STDIN) - 1)

// Special prefix used to load images from external command output
#define LDRSRC_EXEC     "exec://"
#define LDRSRC_EXEC_LEN (sizeof(LDRSRC_EXEC) - 1)

/** Image frame. */
struct imgframe {
    struct pixmap pm; ///< Frame data
    size_t duration;  ///< Frame duration in milliseconds (animation)
};

/** Image meta info. */
struct imginfo {
    char* key;   ///< Key name
    char* value; ///< Value
};

struct imgdata;

/** Decoder specific handlers. */
struct imgdec {
    /**
     * Custom rendering function.
     * @param img image container
     * @param scale scale of the image
     * @param x,y destination left top coordinates
     * @param dst destination pixmap
     */
    void (*render)(struct imgdata* img, double scale, ssize_t x, ssize_t y,
                   struct pixmap* dst);

    /**
     * Custom flip function.
     * @param img image container
     * @param vertical set to true for vertical flip, otherwise horizontal
     */
    void (*flip)(struct imgdata* img, bool vertical);

    /**
     * Custom rotate function.
     * @param img image container
     * @param angle rotation angle (only 90, 180, 270)
     */
    void (*rotate)(struct imgdata* img, size_t angle);

    /**
     * Free decoder internal data.
     * @param img image container
     */
    void (*free)(struct imgdata* img);

    /** Decoder internal data. */
    void* data;
};

/** Image data container. */
struct imgdata {
    char* parent;            ///< Parent directory name
    char* format;            ///< Format description
    bool alpha;              ///< Alpha channel
    struct array* frames;    ///< Frames (RGBA pixmaps)
    struct array* info;      ///< Meta info
    struct pixmap thumbnail; ///< Image thumbnail
    struct imgdec decoder;   ///< Decoder specific handlers
};

/** Image context. */
struct image {
    struct list list; ///< Links to prev/next entry in the image list

    char* source;     ///< Image source (e.g. path to the image file)
    const char* name; ///< Name of the image file
    size_t index;     ///< Index of the image

    size_t file_size; ///< Size of the image file
    time_t file_time; ///< File modification time

    struct imgdata* data; ///< Image data container
};

/** Image loading status. */
enum image_status {
    imgload_success,     ///< Image was decoded successfully
    imgload_unsupported, ///< Unsupported format
    imgload_fmterror,    ///< Invalid data format
    imgload_unknown      ///< Unknown errors
};

/** Image data types. */
#define IMGDATA_FRAMES (1 << 0)
#define IMGDATA_THUMB  (1 << 1)
#define IMGDATA_INFO   (1 << 2)
#define IMGDATA_ALL    (IMGDATA_FRAMES | IMGDATA_THUMB | IMGDATA_INFO)
#define IMGDATA_SELF   (1 << 3 | IMGDATA_ALL)

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
 * Clear image data.
 * @param img image context
 * @param mask data type to clean (`IMGDATA_*`)
 */
bool image_clear(struct image* img, size_t mask);

/**
 * Free image data.
 * @param img image context
 * @param mask data type to free (`IMGDATA_*`)
 */
void image_free(struct image* img, size_t mask);

/**
 * Load image from specified source.
 * @param img image context
 * @return loading status
 */
enum image_status image_load(struct image* img);

/**
 * Attach image data container (move from another instance).
 * @param img target image instance
 * @param from adopted image instance
 */
void image_attach(struct image* img, struct image* from);

/**
 * Export image to a file.
 * @param img image context
 * @param frame frame index
 * @param path path to write the file
 * @return true if image has frame data
 */
bool image_export(const struct image* img, size_t frame, const char* path);

/**
 * Render image.
 * @param img image context
 * @param frame frame index
 * @param scaler scale filter to use
 * @param scale scale of the image
 * @param x,y destination left top coordinates
 * @param dst destination pixmap
 */
void image_render(struct image* img, size_t frame, enum aa_mode scaler,
                  double scale, ssize_t x, ssize_t y, struct pixmap* dst);

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
 * Check if image has meta info data.
 * @param img image context
 * @return true if image has meta info
 */
bool image_has_info(const struct image* img);

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

/**
 * Set image format description.
 * @param img image data container
 * @param fmt format description
 */
void image_set_format(struct imgdata* img, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

/**
 * Add meta info property.
 * @param img image data container
 * @param key property name
 * @param fmt value format
 */
void image_add_info(struct imgdata* img, const char* key, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));

/**
 * Create multiple empty frames.
 * @param img image data container
 * @param num total number of frames
 * @return pointer to the frame array or NULL on errors
 */
struct array* image_alloc_frames(struct imgdata* img, size_t num);

/**
 * Create single frame and allocate pixmap.
 * @param img image data container
 * @param width,height frame size in px
 * @return pointer to the pixmap associated with the frame, or NULL on errors
 */
struct pixmap* image_alloc_frame(struct imgdata* img, size_t width,
                                 size_t height);
