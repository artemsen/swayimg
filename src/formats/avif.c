// SPDX-License-Identifier: MIT
// AV1 (AVIF/AVIFS) format decoder.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#include "../loader.h"

#include <avif/avif.h>
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
#if AVIF_VERSION_MAJOR > 0
    rc =
#endif
        avifRGBImageAllocatePixels(&rgb);
#if AVIF_VERSION_MAJOR > 0
    if (rc != AVIF_RESULT_OK) {
        goto decode_fail;
    }
#endif

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
    return -1;
}

static int decode_frames(struct image* ctx, avifDecoder* decoder)
{
    avifImageTiming timing;
    avifRGBImage rgb = { 0 };
    avifResult rc = AVIF_RESULT_UNKNOWN_ERROR;

    if (!image_create_frames(ctx, decoder->imageCount)) {
        return AVIF_RESULT_UNKNOWN_ERROR;
    }

    for (size_t i = 0; i < ctx->num_frames; ++i) {
        rc = avifDecoderNthImage(decoder, i);
        if (rc != AVIF_RESULT_OK) {
            break;
        }

        avifRGBImageSetDefaults(&rgb, decoder->image);
        rgb.depth = 8;
        rgb.format = AVIF_RGB_FORMAT_BGRA;

#if AVIF_VERSION_MAJOR == 0
        avifRGBImageAllocatePixels(&rgb);
#else
        rc = avifRGBImageAllocatePixels(&rgb);
        if (rc != AVIF_RESULT_OK) {
            break;
        }
#endif

        rc = avifImageYUVToRGB(decoder->image, &rgb);
        if (rc != AVIF_RESULT_OK) {
            break;
        }

        if (!pixmap_create(&ctx->frames[i].pm, rgb.width, rgb.height)) {
            break;
        }

        rc = avifDecoderNthImageTiming(decoder, i, &timing);
        if (rc != AVIF_RESULT_OK) {
            break;
        }

        ctx->frames[i].duration = (size_t)(1000.0f / (float)timing.timescale *
                                           (float)timing.durationInTimescales);

        memcpy(ctx->frames[i].pm.data, rgb.pixels,
               rgb.width * rgb.height * sizeof(argb_t));

        avifRGBImageFreePixels(&rgb);
        rgb.pixels = NULL;
    }

    if (rgb.pixels) {
        avifRGBImageFreePixels(&rgb);
    }

    return rc;
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
    image_free_frames(ctx);
    return ldr_fmterror;
}
