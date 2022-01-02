// SPDX-License-Identifier: MIT
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <cairo/cairo.h>

// Max number of meta entries
#define META_MAX_ENTRY 16

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
    const char* path;           ///< Path to the file
    char* meta[META_MAX_ENTRY]; ///< Image meta info
    orientation_t orientation;  ///< Image orientation
    cairo_surface_t* surface;   ///< Image surface
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
 * Add meta property.
 * @param[in] img image instance
 * @param[in] name property name
 * @param[in] value property value
 */
void add_image_meta(image_t* img, const char* name, const char* value);

/**
 * Get string with the names of the supported image formats.
 * @return image instance or NULL on errors
 */
const char* supported_formats(void);
