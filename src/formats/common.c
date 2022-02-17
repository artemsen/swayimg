// SPDX-License-Identifier: MIT
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "common.h"

#include <stdint.h>
#include <stdio.h>

// Bytes per pixel in ARGB mode
#define BYTES_PER_PIXEL 4

cairo_surface_t* create_surface(size_t width, size_t height, bool alpha)
{
    cairo_surface_t* surface = NULL;

    if (width > MAX_CAIRO_IMAGE_SIZE || height > MAX_CAIRO_IMAGE_SIZE) {
        fprintf(stderr,
                "Unable to create surface: image too big (%d pixels max)\n",
                MAX_CAIRO_IMAGE_SIZE);
    } else {
        const cairo_format_t fmt =
            alpha ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;
        cairo_status_t status;
        surface = cairo_image_surface_create(fmt, width, height);
        status = cairo_surface_status(surface);
        if (status != CAIRO_STATUS_SUCCESS) {
            const char* desc = cairo_status_to_string(status);
            fprintf(stderr, "Unable to create Cairo surface: %s\n",
                    desc ? desc : "Unknown error");
            if (surface) {
                cairo_surface_destroy(surface);
                surface = NULL;
            }
        }
    }

    return surface;
}

/**
 * Apply alpha to color.
 * @param[in] alpha alpha channel value
 * @param[in] color color value
 * @return color with applied alpha
 */
static inline uint8_t multiply_alpha(uint8_t alpha, uint8_t color)
{
    const uint16_t tmp = (alpha * color) + 0x80;
    return ((tmp + (tmp >> 8)) >> 8);
}

void apply_alpha(cairo_surface_t* surface)
{
    uint8_t* data = cairo_image_surface_get_data(surface);
    const size_t width = (size_t)cairo_image_surface_get_width(surface);
    const size_t height = (size_t)cairo_image_surface_get_height(surface);

    for (size_t y = 0; y < height; ++y) {
        uint8_t* line = data + y * width * BYTES_PER_PIXEL;
        for (size_t x = 0; x < width; ++x) {
            uint8_t* pixel = line + x * BYTES_PER_PIXEL;
            const uint8_t alpha = pixel[3];
            if (alpha != 0xff) {
                pixel[0] = multiply_alpha(alpha, pixel[0]);
                pixel[1] = multiply_alpha(alpha, pixel[1]);
                pixel[2] = multiply_alpha(alpha, pixel[2]);
            }
        }
    }

    cairo_surface_mark_dirty(surface);
}
