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
 * Callback for image loader prepare (background thread loader).
 * @param index index of the image to load
 * @return next image index or IMGLIST_INVALID to stop loader
 */
typedef size_t (*load_prepare_fn)(size_t index);

/**
 * Callback for image loader completion (background thread loader).
 * @param image loaded image instance, NULL if no more files to load
 * @param index index of the image in the image list
 * @return next image index or IMGLIST_INVALID to stop loader
 */
typedef size_t (*load_complete_fn)(struct image* image, size_t index);

/**
 * Load image from specified source.
 * @param source image data source: path to the file, exec command, etc
 * @param image pointer to output image instance
 * @return loading status
 */
enum loader_status load_image_source(const char* source, struct image** image);

/**
 * Load image with specified index in the image list.
 * @param index index of the entry in the image list
 * @param image pointer to output image instance
 * @return loading status
 */
enum loader_status load_image(size_t index, struct image** image);

/**
 * Load image in background thread.
 * @param index index of the image in the image list
 * @param on_prepare,on_complete callback functions
 */
void load_image_start(size_t index, load_prepare_fn on_prepare,
                      load_complete_fn on_complete);
/**
 * Stop loader thread.
 */
void load_image_stop(void);
