// SPDX-License-Identifier: MIT
// AV1 (AVIF/AVIFS) format decoder.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include <avif/avif.h>
#include <string.h>

// AVI signature
static const uint32_t signature = 'f' | 't' << 8 | 'y' << 16 | 'p' << 24;
#define SIGNATURE_OFFSET 4

/**
 * Decode current frame.
 * @param avif AVIF decoder context
 * @param pm target pixmap
 * @return error code, AVIF_RESULT_OK on success
 */
static avifResult decode_frame(avifDecoder* avif, struct pixmap* pm)
{
    avifRGBImage rgb = { 0 };
    avifResult rc;

    avifRGBImageSetDefaults(&rgb, avif->image);

    rgb.depth = 8;
    rgb.format = AVIF_RGB_FORMAT_BGRA;

#if AVIF_VERSION_MAJOR == 0
    avifRGBImageAllocatePixels(&rgb);
#else
    rc = avifRGBImageAllocatePixels(&rgb);
    if (rc != AVIF_RESULT_OK) {
        return rc;
    }
#endif

    rc = avifImageYUVToRGB(avif->image, &rgb);
    if (rc == AVIF_RESULT_OK) {
        if (!pixmap_create(pm, avif->alphaPresent ? pixmap_argb : pixmap_xrgb,
                           rgb.width, rgb.height)) {
            rc = AVIF_RESULT_OUT_OF_MEMORY;
        } else {
            memcpy(pm->data, rgb.pixels,
                   rgb.width * rgb.height * sizeof(argb_t));
        }
    }

    avifRGBImageFreePixels(&rgb);

    return rc;
}

// AV1 loader implementation
enum image_status decode_avif(struct imgdata* img, const uint8_t* data,
                              size_t size)
{
    avifDecoder* avif;
    avifResult rc;

    // check signature
    if (size < SIGNATURE_OFFSET + sizeof(signature) ||
        *(const uint32_t*)(data + SIGNATURE_OFFSET) != signature) {
        return imgload_unsupported;
    }

    // open file in decoder
    avif = avifDecoderCreate();
    if (!avif) {
        return imgload_fmterror;
    }
    rc = avifDecoderSetIOMemory(avif, data, size);
    if (rc != AVIF_RESULT_OK) {
        goto done;
    }
    rc = avifDecoderParse(avif);
    if (rc != AVIF_RESULT_OK) {
        goto done;
    }

    if (!image_alloc_frames(img, avif->imageCount)) {
        goto done;
    }

    if (avif->imageCount == 1) {
        // single image
        struct imgframe* frame = arr_nth(img->frames, 0);
        rc = avifDecoderNextImage(avif);
        if (rc != AVIF_RESULT_OK) {
            goto done;
        }
        rc = decode_frame(avif, &frame->pm);
    } else {
        // multiple images
        for (size_t i = 0; i < img->frames->size; ++i) {
            struct imgframe* frame = arr_nth(img->frames, i);
            avifImageTiming timing;

            rc = avifDecoderNthImage(avif, i);
            if (rc != AVIF_RESULT_OK) {
                goto done;
            }
            rc = decode_frame(avif, &frame->pm);
            if (rc != AVIF_RESULT_OK) {
                goto done;
            }

            rc = avifDecoderNthImageTiming(avif, i, &timing);
            if (rc != AVIF_RESULT_OK) {
                goto done;
            }
            frame->duration = (size_t)(1000.0f / (float)timing.timescale *
                                       (float)timing.durationInTimescales);
        }
    }

done:
    if (rc == AVIF_RESULT_OK) {
        image_set_format(img, "AV1 %dbpc %s", avif->image->depth,
                         avifPixelFormatToString(avif->image->yuvFormat));
    }

    avifDecoderDestroy(avif);
    return rc == AVIF_RESULT_OK ? imgload_success : imgload_fmterror;
}
