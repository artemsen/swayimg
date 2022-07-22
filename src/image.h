// SPDX-License-Identifier: MIT
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Orientation (top left corner projection)
typedef enum {
    ori_undefined,
    ori_top_left,
    ori_top_right,
    ori_bottom_right,
    ori_bottom_left,
    ori_left_top,
    ori_right_top,
    ori_right_bottom,
    ori_left_bottom,
} orientation_t;

/** Image description. */
typedef struct {
    const char* path;          ///< Path to the file
    size_t width;              ///< Image width (px)
    size_t height;             ///< Image height (px)
    uint32_t* data;            ///< Pointer to pixel data
    bool alpha;                ///< Has alpha channel?
    const char* info;          ///< Image meta info
    orientation_t orientation; ///< Image orientation
} image_t;

/**
 * Load image from file.
 * @param[in] file path to the file to load
 * @return image instance or NULL on errors
 */
image_t* image_from_file(const char* file);

/**
 * Load image from stdin data.
 * @return image instance or NULL on errors
 */
image_t* image_from_stdin(void);

/**
 * Free image.
 * @param[in] img image instance to free
 */
void image_free(image_t* img);

/**
 * Flip image vertically.
 * @param[in] img image instance
 */
void image_flip(image_t* img);

/**
 * Add meta info property.
 * @param[in] img image instance
 * @param[in] key property name
 * @param[in] fmt value format
 */
void add_image_info(image_t* img, const char* key, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));
