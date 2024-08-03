// SPDX-License-Identifier: MIT
// Image loader.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.h"

// File name used for image, that is read from stdin through pipe
#define LDRSRC_STDIN     "stdin://"
#define LDRSRC_STDIN_LEN (sizeof(LDRSRC_STDIN) - 1)

// Special prefix used to load images from external command output
#define LDRSRC_EXEC     "exec://"
#define LDRSRC_EXEC_LEN (sizeof(LDRSRC_EXEC) - 1)

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
 * @param image target image instance
 * @param data raw image data
 * @param size size of image data in bytes
 * @return loader status
 */
typedef enum loader_status (*image_decoder)(struct image* image,
                                            const uint8_t* data, size_t size);

/**
 * Initialize background thread loader.
 */
void loader_init(void);

/**
 * Destroy background thread loader.
 */
void loader_destroy(void);

/**
 * Load image from specified source.
 * @param source image data source: path to the file, exec command, etc
 * @param image pointer to output image instance
 * @return loading status
 */
enum loader_status loader_from_source(const char* source, struct image** image);

/**
 * Load image with specified index in the image list.
 * @param index index of the entry in the image list
 * @param image pointer to output image instance
 * @return loading status
 */
enum loader_status loader_from_index(size_t index, struct image** image);

/**
 * Append image to background loader queue.
 * @param index index of the image in the image list
 */
void loader_queue_append(size_t index);

/**
 * Reset background loader queue.
 */
void loader_queue_reset(void);
