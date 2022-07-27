// SPDX-License-Identifier: MIT
// AV1 (AVIF) format decoder.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include <avif/avif.h>
#include <string.h>

// HEIF signature
static const uint32_t signature = 'f' | 't' << 8 | 'y' << 16 | 'p' << 24;
#define SIGNATURE_OFFSET 4

// AV1 loader implementation
enum loader_status decode_avif(struct image* ctx, const uint8_t* data,
                               size_t size)
{
    avifResult rc;
    avifRGBImage rgb;
    avifDecoder* decoder = NULL;

    // check signature
    if (size < SIGNATURE_OFFSET + sizeof(signature) ||
        *(const uint32_t*)(data + SIGNATURE_OFFSET) != signature) {
        return ldr_unsupported;
    }

    // open file in decoder
    decoder = avifDecoderCreate();
    if (!decoder) {
        image_error(ctx, "unable to create av1 decoder");
        return ldr_fmterror;
    }
    rc = avifDecoderSetIOMemory(decoder, data, size);
    if (rc != AVIF_RESULT_OK) {
        goto fail_decoder;
    }
    rc = avifDecoderParse(decoder);
    if (rc != AVIF_RESULT_OK) {
        goto fail_decoder;
    }
    rc = avifDecoderNextImage(decoder); // first frame only
    if (rc != AVIF_RESULT_OK) {
        goto fail_decoder;
    }

    // setup decoder
    memset(&rgb, 0, sizeof(rgb));
    avifRGBImageSetDefaults(&rgb, decoder->image);
    rgb.depth = 8;
    rgb.format = AVIF_RGB_FORMAT_BGRA;
    avifRGBImageAllocatePixels(&rgb);

    // decode the frame
    rc = avifImageYUVToRGB(decoder->image, &rgb);
    if (rc != AVIF_RESULT_OK) {
        goto fail_pixels;
    }

    if (!image_allocate(ctx, rgb.width, rgb.height)) {
        rc = AVIF_RESULT_UNKNOWN_ERROR;
        goto fail_pixels;
    }
    memcpy((void*)ctx->data, rgb.pixels,
           ctx->width * ctx->height * sizeof(argb_t));

    ctx->alpha = true;
    image_add_meta(ctx, "Format", "AV1 %dbpc %s", decoder->image->depth,
                   avifPixelFormatToString(decoder->image->yuvFormat));

    avifRGBImageFreePixels(&rgb);
    avifDecoderDestroy(decoder);
    return ldr_success;

fail_pixels:
    avifRGBImageFreePixels(&rgb);
fail_decoder:
    avifDecoderDestroy(decoder);
    if (rc != AVIF_RESULT_OK) {
        image_error(ctx, "error decoding av1: %s\n", avifResultToString(rc));
    }
    return ldr_fmterror;
}
