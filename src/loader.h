// SPDX-License-Identifier: MIT
// Image loader and cache.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.h"

// File name used for image, that is read from stdin through pipe
#define STDIN_FILE_NAME "{STDIN}"

/** Loader status. */
enum loader_status {
    ldr_success,     ///< Image was decoded successfully
    ldr_unsupported, ///< Unsupported format
    ldr_fmterror,    ///< Invalid data format
    ldr_ioerror      ///< IO errors
};

/** Contains string with the names of the supported image formats. */
extern const char* supported_formats;

/**
 * Image loader function prototype, implemented by decoders.
 * @param ctx image context
 * @param data raw image data
 * @param size size of image data in bytes
 * @return loader status
 */
typedef enum loader_status (*image_decoder)(struct image* ctx,
                                            const uint8_t* data, size_t size);

/**
 * Create loader instance.
 */
void loader_create(void);

/**
 * Initialize loader: create caches, load first image, etc.
 * @param start initial index of image in the image list
 * @param force mandatory image index flag
 * @return true if image was loaded
 */
bool loader_init(size_t start, bool force);

/**
 * Free loader resources: destroy caches etc.
 */
void loader_free(void);

/**
 * Reset cache and reload current image.
 * @return false if reloading current image failed
 */
bool loader_reset(void);

/**
 * Load image from specified source.
 * @param source image data source: path the file, stdio, etc
 * @param status fail reason, can be NULL
 * @return image context or NULL on errors
 */
struct image* loader_load_image(const char* source, enum loader_status* status);

/**
 * Get (may be load) image for specified index
 * @param index index of the image in the image list
 * @return image context or NULL on errors
 */
struct image* loader_get_image(size_t index);

/**
 * Get current image (last requested by `loader_get_image`).
 * @return current image or NULL if no image loaded
 */
struct image* loader_current_image(void);

/**
 * Get current image index (last requested by `loader_get_image`).
 * @return current image index
 */
size_t loader_current_index(void);
