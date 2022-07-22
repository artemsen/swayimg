// SPDX-License-Identifier: MIT
// WebP format decoder.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include <string.h>
#include <webp/decode.h>

// WebP signature
static const uint8_t signature[] = { 'R', 'I', 'F', 'F' };

// WebP loader implementation
bool load_webp(image_t* img, const uint8_t* data, size_t size)
{
    WebPBitstreamFeatures prop;
    VP8StatusCode status;
    int stride;

    // check signature
    if (size < sizeof(signature) ||
        memcmp(data, signature, sizeof(signature))) {
        return false;
    }

    // get image properties
    status = WebPGetFeatures(data, size, &prop);
    if (status != VP8_STATUS_OK) {
        image_error(img, "unable to get webp properties, error %d\n", status);
        return false;
    }

    if (!image_allocate(img, prop.width, prop.height)) {
        return false;
    }

    // decode image
    stride = img->width * sizeof(img->data[0]);
    if (!WebPDecodeBGRAInto(data, size, (uint8_t*)img->data,
                            stride * img->height, stride)) {
        image_error(img, "unable to decode webp image");
        image_deallocate(img);
        return false;
    }

    if (prop.has_alpha) {
        image_apply_alpha(img);
        img->alpha = true;
    }
    add_image_info(
        img, "Format", "WebP %s %s%s", prop.format == 1 ? "lossy" : "lossless",
        prop.has_alpha ? "+alpha" : "", prop.has_animation ? "+animation" : "");

    return true;
}
