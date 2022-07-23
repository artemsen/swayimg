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
 * Decode image from memory buffer.
 * @param ctx image context
 * @param data raw image data
 * @param size size of image data in bytes
 * @return loader status
 */
enum loader_status image_decode(struct image* ctx, const uint8_t* data,
                                size_t size);

/**
 * Allocate buffer for image data.
 * @param ctx image context
 * @param width image width in px
 * @param height image height in px
 * @return false if error
 */
bool image_allocate(struct image* ctx, size_t width, size_t height);

/**
 * Free buffer of image data.
 * @param ctx image context
 */
void image_deallocate(struct image* ctx);

/**
 * Print description of decoding problem.
 * @param ctx image context
 * @param fmt text format
 */
void image_error(const struct image* ctx, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

/**
 * Get string with the names of the supported image formats.
 * @return list of supported format
 */
const char* supported_formats(void);
