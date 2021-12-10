// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

//
// WebP image format support
//

#include "config.h"
#ifndef HAVE_LIBWEBP
#error Invalid build configuration
#endif

#include "../image.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <webp/decode.h>

// WebP signature
static const uint8_t signature[] = { 'R', 'I', 'F', 'F' };

static uint8_t multiply_alpha(uint8_t alpha, uint8_t color)
{
    const uint16_t temp = (alpha * color) + 0x80;
    return ((temp + (temp >> 8)) >> 8);
}

// WebP loader implementation
struct image* load_webp(const uint8_t* data, size_t size)
{
    // check signature
    if (size < sizeof(signature) ||
        memcmp(data, signature, sizeof(signature))) {
        return NULL;
    }

    // get image properties
    WebPBitstreamFeatures prop;
    VP8StatusCode status = WebPGetFeatures(data, size, &prop);
    if (status != VP8_STATUS_OK) {
        fprintf(stderr, "Unable to get WebP image properties: status %i\n",
                status);
        return NULL;
    }

    // format description
    char format[32];
    strcpy(format, "WebP");
    if (prop.format == 1) {
        strcat(format, " lossy");
    } else if (prop.format == 2) {
        strcat(format, " lossless");
    }
    if (prop.has_alpha) {
        strcat(format, " +alpha");
    }
    if (prop.has_animation) {
        strcat(format, " +animation");
    }

    // create image instance
    const cairo_format_t fmt =
        prop.has_alpha ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;
    struct image* img = create_image(fmt, prop.width, prop.height);
    if (!img) {
        return NULL;
    }
    set_image_meta(img, "%s", format);

    uint8_t* dst_data = cairo_image_surface_get_data(img->surface);
    const size_t stride = cairo_image_surface_get_stride(img->surface);
    const size_t len = stride * prop.height;
    if (!WebPDecodeBGRAInto(data, size, dst_data, len, stride)) {
        fprintf(stderr, "Error decoding WebP\n");
        free_image(img);
        return NULL;
    }

    // handle transparency
    if (prop.has_alpha) {
        for (size_t i = 0; i < len; i += 4 /* argb */) {
            const uint8_t alpha = dst_data[i + 3];
            if (alpha != 0xff) {
                dst_data[i + 0] = multiply_alpha(alpha, dst_data[i + 0]);
                dst_data[i + 1] = multiply_alpha(alpha, dst_data[i + 1]);
                dst_data[i + 2] = multiply_alpha(alpha, dst_data[i + 2]);
            }
        }
    }

    cairo_surface_mark_dirty(img->surface);

    return img;
}
