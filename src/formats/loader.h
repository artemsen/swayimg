// SPDX-License-Identifier: MIT
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "../image.h"

/**
 * Get string with the names of the supported image formats.
 * @return list of supported format
 */
const char* supported_formats(void);

/**
 * Decode image from memory buffer.
 * @param[in] img image instance
 * @param[in] data raw image data
 * @param[in] size size of image data in bytes
 * @return true if image was loaded
 */
bool image_decode(image_t* img, const uint8_t* data, size_t size);

/**
 * Allocate buffer for image data.
 * @param[in] img image instance
 * @param[in] width image width in px
 * @param[in] height image height in px
 * @return false if error
 */
bool image_allocate(image_t* img, size_t width, size_t height);

/**
 * Free buffer of image data.
 * @param[in] img image instance
 */
void image_deallocate(image_t* img);

/**
 * Print description of decoding problem.
 * @param[in] img image instance
 * @param[in] fmt text format
 */
void image_error(const image_t* img, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

/**
 * Apply alpha channel.
 * @param[in] img image instance
 */
void image_apply_alpha(image_t* img);
