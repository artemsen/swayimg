// SPDX-License-Identifier: MIT
// AV1 (AVIF/AVIFS) format decoder.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include <avif/avif.h>
#include <string.h>

// AVI signature
static const uint32_t signature = 'f' | 't' << 8 | 'y' << 16 | 'p' << 24;
#define SIGNATURE_OFFSET 4

static int decode_frame(struct image* img, avifDecoder* decoder)
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

    pm = image_alloc_frame(img, decoder->image->width, decoder->image->height);
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

static int decode_frames(struct image* img, avifDecoder* decoder)
{
    avifImageTiming timing;
    avifRGBImage rgb = { 0 };
    avifResult rc = AVIF_RESULT_UNKNOWN_ERROR;

    if (!image_alloc_frames(img, decoder->imageCount)) {
        return AVIF_RESULT_UNKNOWN_ERROR;
    }

    for (size_t i = 0; i < img->num_frames; ++i) {
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

        if (!pixmap_create(&img->frames[i].pm, rgb.width, rgb.height)) {
            break;
        }

        rc = avifDecoderNthImageTiming(decoder, i, &timing);
        if (rc != AVIF_RESULT_OK) {
            break;
        }

        img->frames[i].duration = (size_t)(1000.0f / (float)timing.timescale *
                                           (float)timing.durationInTimescales);

        memcpy(img->frames[i].pm.data, rgb.pixels,
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
enum image_status decode_avif(struct image* img, const uint8_t* data,
                              size_t size)
{
    avifResult rc;
    avifDecoder* decoder = NULL;
    int ret;

    // check signature
    if (size < SIGNATURE_OFFSET + sizeof(signature) ||
        *(const uint32_t*)(data + SIGNATURE_OFFSET) != signature) {
        return imgload_unsupported;
    }

    // open file in decoder
    decoder = avifDecoderCreate();
    if (!decoder) {
        return imgload_fmterror;
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
        ret = decode_frames(img, decoder);
    } else {
        ret = decode_frame(img, decoder);
    }

    if (ret != 0) {
        goto fail;
    }

    img->alpha = decoder->alphaPresent;

    image_set_format(img, "AV1 %dbpc %s", decoder->image->depth,
                     avifPixelFormatToString(decoder->image->yuvFormat));

    avifDecoderDestroy(decoder);
    return imgload_success;

fail:
    avifDecoderDestroy(decoder);
    image_free(img, IMGFREE_FRAMES);
    return imgload_fmterror;
}
