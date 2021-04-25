// SPDX-License-Identifier: MIT
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stddef.h>
#include <cairo/cairo.h>

/** Image description. */
struct image {
    /** Image format description. */
    const char* format;
    /** Image surface. */
    cairo_surface_t* surface;
};

/**
 * Load image from file.
 * @param[in] file path to the file to load
 * @return image instance or NULL on errors
 */
struct image* load_image(const char* file);

/**
 * Free image.
 * @param[in] img image instance to free
 */
void free_image(struct image* img);

/**
 * Construct image instance (helper function for loaders).
 * @param[in] color color format (24/32 bpp)
 * @param[in] width width of the image
 * @param[in] height height of the image
 * @param[in] format image format description
 * @param[in] ... data for format description
 */
__attribute__((format (printf, 4, 5)))
struct image* create_image(cairo_format_t color, size_t width, size_t height,
                           const char* format, ...);
