// SPDX-License-Identifier: MIT
// WebP format decoder.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include <string.h>
#include <webp/decode.h>

// WebP signature
static const uint8_t signature[] = { 'R', 'I', 'F', 'F' };

// WebP loader implementation
enum loader_status decode_webp(struct image* ctx, const uint8_t* data,
                               size_t size)
{
    WebPBitstreamFeatures prop;
    VP8StatusCode status;
    int stride;

    // check signature
    if (size < sizeof(signature) ||
        memcmp(data, signature, sizeof(signature))) {
        return ldr_unsupported;
    }

    // get image properties
    status = WebPGetFeatures(data, size, &prop);
    if (status != VP8_STATUS_OK) {
        image_error(ctx, "unable to get webp properties, error %d\n", status);
        return ldr_fmterror;
    }

    if (!image_allocate(ctx, prop.width, prop.height)) {
        return ldr_fmterror;
    }

    // decode image
    stride = ctx->width * sizeof(argb_t);
    if (!WebPDecodeBGRAInto(data, size, (uint8_t*)ctx->data,
                            stride * ctx->height, stride)) {
        image_error(ctx, "unable to decode webp image");
        image_deallocate(ctx);
        return ldr_fmterror;
    }

    if (prop.has_alpha) {
        ctx->alpha = true;
    }
    image_add_meta(
        ctx, "Format", "WebP %s %s%s", prop.format == 1 ? "lossy" : "lossless",
        prop.has_alpha ? "+alpha" : "", prop.has_animation ? "+animation" : "");

    return ldr_success;
}
