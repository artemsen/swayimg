// SPDX-License-Identifier: MIT
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stddef.h>
#include <cairo/cairo.h>

/** Image description. */
struct image {
    /** File name. */
    const char* name;
    /** Format description. */
    const char* format;
    /** Image surface. */
    cairo_surface_t* surface;
};

/**
 * Construct image instance.
 * @param[in] color color format (24/32 bpp)
 * @param[in] width width of the image
 * @param[in] height height of the image
 */
struct image* create_image(cairo_format_t color, size_t width, size_t height);

/**
 * Set image meta info (format description).
 * @param[in] img image instance
 * @param[in] format image format description
 * @param[in] ... data for format description
 */
__attribute__((format (printf, 2, 3)))
void set_image_meta(struct image* img, const char* format, ...);

/**
 * Free image.
 * @param[in] img image instance to free
 */
void free_image(struct image* img);
