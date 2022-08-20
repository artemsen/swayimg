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
    struct image_frame* frame;
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
        image_print_error(ctx, "unable to get webp properties, error %d\n",
                          status);
        return ldr_fmterror;
    }

    frame = image_create_frame(ctx, prop.width, prop.height);
    if (!frame) {
        return ldr_fmterror;
    }

    // decode image
    stride = frame->width * sizeof(argb_t);
    if (!WebPDecodeBGRAInto(data, size, (uint8_t*)frame->data,
                            stride * frame->height, stride)) {
        image_print_error(ctx, "unable to decode webp image");
        image_free_frames(ctx);
        return ldr_fmterror;
    }

    image_set_format(
        ctx, "WebP %s %s%s", prop.format == 1 ? "lossy" : "lossless",
        prop.has_alpha ? "+alpha" : "", prop.has_animation ? "+animation" : "");
    ctx->alpha = prop.has_alpha;

    return ldr_success;
}
