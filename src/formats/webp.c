// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

//
// WebP image format support
//

#include <cairo/cairo.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <webp/decode.h>

// WebP signature
static const uint8_t signature[] = { 'R', 'I', 'F', 'F' };

/**
 * Apply alpha to color.
 * @param[in] alpha alpha channel value
 * @param[in] color color value
 * @return color with applied alpha
 */
static uint8_t multiply_alpha(uint8_t alpha, uint8_t color)
{
    const uint16_t temp = (alpha * color) + 0x80;
    return ((temp + (temp >> 8)) >> 8);
}

// WebP loader implementation
cairo_surface_t* load_webp(const uint8_t* data, size_t size, char* format,
                           size_t format_sz)
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

    // create image instance
    const cairo_format_t fmt =
        prop.has_alpha ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;
    cairo_surface_t* surface =
        cairo_image_surface_create(fmt, prop.width, prop.height);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Unable to create surface\n");
        return NULL;
    }
    snprintf(format, format_sz, "WebP %s %s%s",
             prop.format == 1 ? "lossy" : "lossless",
             prop.has_alpha ? "+alpha" : "",
             prop.has_animation ? "+animation" : "");

    uint8_t* dst_data = cairo_image_surface_get_data(surface);
    const size_t stride = cairo_image_surface_get_stride(surface);
    const size_t len = stride * prop.height;
    if (!WebPDecodeBGRAInto(data, size, dst_data, len, stride)) {
        fprintf(stderr, "Error decoding WebP\n");
        cairo_surface_destroy(surface);
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

    cairo_surface_mark_dirty(surface);

    return surface;
}
