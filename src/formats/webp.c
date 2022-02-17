// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

//
// WebP image format support
//

#include "common.h"

#include <cairo/cairo.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <webp/decode.h>

// WebP signature
static const uint8_t signature[] = { 'R', 'I', 'F', 'F' };

// WebP loader implementation
cairo_surface_t* load_webp(const uint8_t* data, size_t size, char* format,
                           size_t format_sz)
{
    cairo_surface_t* surface = NULL;
    uint8_t* sdata;
    size_t stride;
    WebPBitstreamFeatures prop;
    VP8StatusCode status;

    // check signature
    if (size < sizeof(signature) ||
        memcmp(data, signature, sizeof(signature))) {
        return NULL;
    }

    // get image properties
    status = WebPGetFeatures(data, size, &prop);
    if (status != VP8_STATUS_OK) {
        fprintf(stderr, "Unable to get WebP image properties: status %i\n",
                status);
        return NULL;
    }

    // prepare surface and metadata
    surface = create_surface(prop.width, prop.height, prop.has_alpha);
    if (!surface) {
        return NULL;
    }
    snprintf(format, format_sz, "WebP %s %s%s",
             prop.format == 1 ? "lossy" : "lossless",
             prop.has_alpha ? "+alpha" : "",
             prop.has_animation ? "+animation" : "");

    // decode image
    sdata = cairo_image_surface_get_data(surface);
    stride = cairo_image_surface_get_stride(surface);
    if (!WebPDecodeBGRAInto(data, size, sdata, stride * prop.height, stride)) {
        fprintf(stderr, "Error decoding WebP\n");
        cairo_surface_destroy(surface);
        return NULL;
    }

    // handle transparency
    if (prop.has_alpha) {
        apply_alpha(surface);
    }

    cairo_surface_mark_dirty(surface);

    return surface;
}
