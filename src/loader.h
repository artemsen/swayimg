// SPDX-License-Identifier: MIT
// Image loader: interface and common framework for decoding images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.h"

/** Loader status. */
enum loader_status {
    ldr_success,     ///< Image was decoded successfully
    ldr_unsupported, ///< Signature not recognized by any decoder
    ldr_fmterror     ///< Decoder found, but data has invalid format
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
 * Load image from memory buffer.
 * @param ctx image context
 * @param data raw image data
 * @param size size of image data in bytes
 * @return loader status
 */
enum loader_status load_image(struct image* ctx, const uint8_t* data,
                              size_t size);
