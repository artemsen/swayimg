// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stdint.h>
#include <cairo/cairo.h>

/** Image description. */
struct image {
    const char* format;
    cairo_surface_t* image;
};

/**
 * Load image from file.
 * @param[in] file path to the file to load
 * @return image description or NULL if file format is not supported
 */
struct image* load_image(const char* file);

/**
 * Free image.
 * @param[in] img image description
 */
void free_image(struct image* img);
