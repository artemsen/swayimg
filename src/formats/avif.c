// SPDX-License-Identifier: MIT
// AV1 (AVIF/AVIFS) format decoder.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#include "loader.h"
#include "src/image.h"

#include <avif/avif.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// AVI signature
static const uint32_t signature = 'f' | 't' << 8 | 'y' << 16 | 'p' << 24;
#define SIGNATURE_OFFSET 4

static int decode_frame(struct image* ctx, avifDecoder* decoder)
{
    avifRGBImage rgb;
    avifResult rc;
    struct pixmap* pm;

    rc = avifDecoderNextImage(decoder);
    if (rc != AVIF_RESULT_OK) {
        goto decode_fail;
    }

    memset(&rgb, 0, sizeof(rgb));
    avifRGBImageSetDefaults(&rgb, decoder->image);

    rgb.depth = 8;
    rgb.format = AVIF_RGB_FORMAT_BGRA;
    avifRGBImageAllocatePixels(&rgb);

    rc = avifImageYUVToRGB(decoder->image, &rgb);
    if (rc != AVIF_RESULT_OK) {
        goto fail_pixels;
    }

    pm = image_allocate_frame(ctx, decoder->image->width,
                              decoder->image->height);
    if (!pm) {
        goto fail_pixels;
    }

    memcpy(pm->data, rgb.pixels, rgb.width * rgb.height * sizeof(argb_t));

    avifRGBImageFreePixels(&rgb);
    return 0;

fail_pixels:
    avifRGBImageFreePixels(&rgb);
decode_fail:
    image_print_error(ctx, "AV1 decode failed: %s", avifResultToString(rc));
    return -1;
}

static int decode_frames(struct image* ctx, avifDecoder* decoder)
{
    avifImageTiming timing;
    avifRGBImage rgb;
    avifResult rc;

    if (!image_create_frames(ctx, decoder->imageCount)) {
        goto decode_fail;
    }

    for (size_t i = 0; i < ctx->num_frames; ++i) {
        rc = avifDecoderNthImage(decoder, i);
        if (rc != AVIF_RESULT_OK) {
            goto decode_fail;
        }

        avifRGBImageSetDefaults(&rgb, decoder->image);
        rgb.depth = 8;
        rgb.format = AVIF_RGB_FORMAT_BGRA;

        avifRGBImageAllocatePixels(&rgb);

        rc = avifImageYUVToRGB(decoder->image, &rgb);
        if (rc != AVIF_RESULT_OK) {
            goto fail_pixels;
        }

        if (!pixmap_create(&ctx->frames[i].pm, rgb.width, rgb.height)) {
            goto fail_pixels;
        }

        rc = avifDecoderNthImageTiming(decoder, i, &timing);
        if (rc != AVIF_RESULT_OK) {
            goto fail_pixels;
        }

        ctx->frames[i].duration = (size_t)(1000.0f / (float)timing.timescale *
                                           (float)timing.durationInTimescales);

        memcpy(ctx->frames[i].pm.data, rgb.pixels,
               rgb.width * rgb.height * sizeof(argb_t));

        avifRGBImageFreePixels(&rgb);
    }

    return 0;

fail_pixels:
    avifRGBImageFreePixels(&rgb);
decode_fail:
    return -1;
}

// AV1 loader implementation
enum loader_status decode_avif(struct image* ctx, const uint8_t* data,
                               size_t size)
{
    avifResult rc;
    avifDecoder* decoder = NULL;
    int ret;

    // check signature
    if (size < SIGNATURE_OFFSET + sizeof(signature) ||
        *(const uint32_t*)(data + SIGNATURE_OFFSET) != signature) {
        return ldr_unsupported;
    }

    // open file in decoder
    decoder = avifDecoderCreate();
    if (!decoder) {
        image_print_error(ctx, "unable to create av1 decoder");
        return ldr_fmterror;
    }
    rc = avifDecoderSetIOMemory(decoder, data, size);
    if (rc != AVIF_RESULT_OK) {
        goto fail;
    }
    rc = avifDecoderParse(decoder);
    if (rc != AVIF_RESULT_OK) {
        goto fail;
    }

    if (decoder->imageCount > 1) {
        ret = decode_frames(ctx, decoder);
    } else {
        ret = decode_frame(ctx, decoder);
    }

    if (ret != 0) {
        goto fail;
    }

    ctx->alpha = decoder->alphaPresent;

    image_set_format(ctx, "AV1 %dbpc %s", decoder->image->depth,
                     avifPixelFormatToString(decoder->image->yuvFormat));

    avifDecoderDestroy(decoder);
    return ldr_success;

fail:
    avifDecoderDestroy(decoder);
    if (rc != AVIF_RESULT_OK) {
        image_print_error(ctx, "error decoding av1: %s\n",
                          avifResultToString(rc));
    }
    image_free_frames(ctx);
    return ldr_fmterror;
}
