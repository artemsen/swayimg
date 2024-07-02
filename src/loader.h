// SPDX-License-Identifier: MIT
// Image loader: interface and common framework for decoding images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

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
 * Get (may be load) image from specified source.
 * @param source image data source: path the file, stdio, etc
 * @return image context or NULL on errors
 */
struct image* loader_get_image(const char* source);
