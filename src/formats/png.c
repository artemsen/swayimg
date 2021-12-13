// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

//
// PNG image format support
//

#include "config.h"
#ifndef HAVE_LIBPNG
#error Invalid build configuration
#endif

#include "../image.h"

#include <png.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uint8_t multiply_alpha(uint8_t alpha, uint8_t color)
{
    const uint16_t temp = (alpha * color) + 0x80;
    return ((temp + (temp >> 8)) >> 8);
}

// PNG loader implementation
struct image* load_png(const uint8_t* data, size_t size)
{
    struct image* img = NULL;
    png_image png;
    uint8_t* buffer;
    png_int_32 stride;

    // check signature
    if (png_sig_cmp(data, 0, size) != 0) {
        return NULL;
    }

    memset(&png, 0, sizeof(png));
    png.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&png, data, size)) {
        fprintf(stderr, "PNG decode failed\n");
        return NULL;
    }

    // set destination format
    png.format = PNG_FORMAT_BGRA;

    // create image instance
    img = create_image(CAIRO_FORMAT_ARGB32, png.width, png.height);
    if (!img) {
        goto error;
    }
    set_image_meta(img, "PNG");

    // decode image
    buffer = cairo_image_surface_get_data(img->surface);
    stride = cairo_image_surface_get_stride(img->surface);
    if (!png_image_finish_read(&png, NULL, buffer, stride, NULL)) {
        fprintf(stderr, "PNG decode failed\n");
        goto error;
    }

    // handle transparency
    for (size_t i = 0; i < png.width * png.height; ++i) {
        uint8_t* pixel = buffer + i * 4;
        const uint8_t alpha = pixel[3];
        if (alpha != 0xff) {
            pixel[0] = multiply_alpha(alpha, pixel[0]);
            pixel[1] = multiply_alpha(alpha, pixel[1]);
            pixel[2] = multiply_alpha(alpha, pixel[2]);
        }
    }

    cairo_surface_mark_dirty(img->surface);

    return img;

error:
    png_image_free(&png);
    free_image(img);
    return NULL;
}
