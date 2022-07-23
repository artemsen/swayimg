// SPDX-License-Identifier: MIT
// AV1 (AVIF) format decoder.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include <avif/avif.h>
#include <string.h>

// HEIF signature
static const uint8_t signature[] = { 'f', 't', 'y', 'p' };
// Ignore first 4 bytes in header
#define SIGNATURE_START 4

// Number of components of rgba pixel
#define RGBA_NUM 4

// AV1 loader implementation
enum loader_status decode_avif(struct image* ctx, const uint8_t* data,
                               size_t size)
{
    avifResult rc;
    avifRGBImage rgb;
    avifDecoder* decoder = NULL;

    // check signature
    if (size < SIGNATURE_START + sizeof(signature) ||
        memcmp(data + SIGNATURE_START, signature, sizeof(signature))) {
        return ldr_unsupported;
    }

    // open file in decoder
    decoder = avifDecoderCreate();
    if (!decoder) {
        image_error(ctx, "unable to create av1 decoder");
        return ldr_fmterror;
    }
    rc = avifDecoderSetIOMemory(decoder, data, size);
    if (rc == AVIF_RESULT_OK) {
        rc = avifDecoderParse(decoder);
    }
    if (rc == AVIF_RESULT_OK) {
        rc = avifDecoderNextImage(decoder); // first frame only
    }
    if (rc != AVIF_RESULT_OK) {
        image_error(ctx, "error decoding av1: %s\n", avifResultToString(rc));
        goto done;
    }

    // setup decoder
    memset(&rgb, 0, sizeof(rgb));
    avifRGBImageSetDefaults(&rgb, decoder->image);
    rgb.format = AVIF_RGB_FORMAT_BGRA;
    avifRGBImageAllocatePixels(&rgb);

    // decode the frame
    rc = avifImageYUVToRGB(decoder->image, &rgb);
    if (rc != AVIF_RESULT_OK) {
        image_error(ctx, "unable convert av1 colors: %s\n",
                    avifResultToString(rc));
        goto done;
    }

    if (!image_allocate(ctx, rgb.width, rgb.height)) {
        rc = AVIF_RESULT_UNKNOWN_ERROR;
        goto done;
    }

    // put image on to cairo surface
    if (rgb.depth == 8) {
        // simple 8bit image
        memcpy(ctx->data, rgb.pixels,
               ctx->width * ctx->height * sizeof(argb_t));
    } else {
        // convert to 8bit image
        const size_t max_clr = 1 << rgb.depth;
        const size_t src_stride =
            rgb.width * sizeof(uint32_t) * sizeof(uint16_t);
        for (size_t y = 0; y < rgb.height; ++y) {
            const uint16_t* src_y =
                (const uint16_t*)(rgb.pixels + y * src_stride);
            uint8_t* dst_y = (uint8_t*)&ctx->data[y * ctx->width];
            for (size_t x = 0; x < rgb.width; ++x) {
                uint8_t* dst_x = (uint8_t*)&dst_y[x];
                const uint16_t* src_x = src_y + x * sizeof(uint32_t);
                dst_x[0] = (uint8_t)((float)src_x[0] / max_clr * 255);
                dst_x[1] = (uint8_t)((float)src_x[1] / max_clr * 255);
                dst_x[2] = (uint8_t)((float)src_x[2] / max_clr * 255);
                dst_x[3] = (uint8_t)((float)src_x[3] / max_clr * 255);
            }
        }
    }

    ctx->alpha = true;
    image_add_meta(ctx, "Format", "AV1 %dbit %s", rgb.depth,
                   avifPixelFormatToString(decoder->image->yuvFormat));

done:
    avifRGBImageFreePixels(&rgb);
    avifDecoderDestroy(decoder);
    return (rc == AVIF_RESULT_OK ? ldr_success : ldr_fmterror);
}
