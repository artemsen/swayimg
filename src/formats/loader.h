// SPDX-License-Identifier: MIT
// Image loader.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "../image.h"

/**
 * Image loader function prototype, implemented by decoders.
 * @param img target image instance
 * @param data raw image data
 * @param size size of image data in bytes
 * @return loader status
 */
typedef enum image_status (*image_decoder)(struct image* img,
                                           const uint8_t* data, size_t size);

/**
 * Set image format description.
 * @param img image context
 * @param fmt format description
 */
void image_set_format(struct image* img, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

/**
 * Add meta info property.
 * @param img image context
 * @param key property name
 * @param fmt value format
 */
void image_add_meta(struct image* img, const char* key, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));

/**
 * Create single frame, allocate buffer and add frame to the image.
 * @param img image context
 * @param width,height frame size in px
 * @return pointer to the pixmap associated with the frame, or NULL on errors
 */
struct pixmap* image_alloc_frame(struct image* img, size_t width,
                                 size_t height);

/**
 * Create list of empty frames.
 * @param img image context
 * @param num total number of frames
 * @return false on errors
 */
bool image_alloc_frames(struct image* img, size_t num);
