// SPDX-License-Identifier: MIT
// Image loader: interface and common framework for decoding images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "../image.h"

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
                                            const uint8_t* data, size_t size,
                                            size_t max_w, size_t max_h);

/**
 * Load image from memory buffer. If max_w and max_h are specified (both non
 * zero), the loader will attempt to decode the image to the closest resolution
 * that's at least as big as the specified target, respecting the aspect ratio
 * of the source image. This is only done if the decoder supports downsampling
 * on decode (eg jpg does)
 * @param ctx image context
 * @param data raw image data
 * @param size size of image data in bytes
 * @param max_w Maximum render width for this image
 * @param max_h Maximum render height for this image
 * @return loader status
 */
enum loader_status load_image(struct image* ctx, const uint8_t* data,
                              size_t size, size_t max_w, size_t max_h);

/**
 * Print decoding problem description.
 * @param ctx image context
 * @param fmt text format
 */
void image_print_error(const struct image* ctx, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));
