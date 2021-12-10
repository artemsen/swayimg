// SPDX-License-Identifier: MIT
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

//
// AV1 image format support (AVIF)
//

#include "config.h"
#ifndef HAVE_LIBAVIF
#error Invalid build configuration
#endif

#include "../image.h"

#include <avif/avif.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// HEIF signature
static const uint8_t signature[] = { 'f', 't', 'y', 'p' };
// Ignore first 4 bytes in header
#define SIGNATURE_START 4

// Number of components of rgba pixel
#define RGBA_NUM 4

// AV1 loader implementation
struct image* load_avif(const uint8_t* data, size_t size)
{
    avifResult rc;
    avifRGBImage rgb;
    avifDecoder* decoder = NULL;
    struct image* img = NULL;

    memset(&rgb, 0, sizeof(rgb));

    // check signature
    if (size < SIGNATURE_START + sizeof(signature) ||
        memcmp(data + SIGNATURE_START, signature, sizeof(signature))) {
        return NULL;
    }

    // open file in decoder
    decoder = avifDecoderCreate();
    if (!decoder) {
        fprintf(stderr, "Error creating AV1 decoder\n");
        return NULL;
    }
    rc = avifDecoderSetIOMemory(decoder, data, size);
    if (rc == AVIF_RESULT_OK) {
        rc = avifDecoderParse(decoder);
    }
    if (rc == AVIF_RESULT_OK) {
        rc = avifDecoderNextImage(decoder); // first frame only
    }
    if (rc != AVIF_RESULT_OK) {
        fprintf(stderr, "Error decoding AV1: %s\n", avifResultToString(rc));
        goto done;
    }

    // setup decoder
    avifRGBImageSetDefaults(&rgb, decoder->image);
    rgb.format = AVIF_RGB_FORMAT_BGRA;
    avifRGBImageAllocatePixels(&rgb);

    // decode the frame
    rc = avifImageYUVToRGB(decoder->image, &rgb);
    if (rc != AVIF_RESULT_OK) {
        fprintf(stderr, "YUV to RGB failed: %s", avifResultToString(rc));
        goto done;
    }

    // create image instance
    img = create_image(CAIRO_FORMAT_ARGB32, rgb.width, rgb.height);
    if (!img) {
        goto done;
    }
    set_image_meta(img, "AV1 %dbit %s", rgb.depth,
                   avifPixelFormatToString(decoder->image->yuvFormat));

    // put image on to cairo surface
    uint8_t* sdata = cairo_image_surface_get_data(img->surface);
    if (rgb.depth == 8) {
        // simple 8bit image
        memcpy(sdata, rgb.pixels, rgb.width * rgb.height * RGBA_NUM);
    } else {
        // convert to 8bit image
        const size_t max_clr = 1 << rgb.depth;
        const size_t src_stride = rgb.width * RGBA_NUM * sizeof(uint16_t);
        const size_t dst_stride = cairo_image_surface_get_stride(img->surface);
        for (size_t y = 0; y < rgb.height; ++y) {
            const uint16_t* src_y =
                (const uint16_t*)(rgb.pixels + y * src_stride);
            uint8_t* dst_y = sdata + y * dst_stride;
            for (size_t x = 0; x < rgb.width; ++x) {
                uint8_t* dst_x = dst_y + x * RGBA_NUM;
                const uint16_t* src_x = src_y + x * RGBA_NUM;
                dst_x[0] = (uint8_t)((float)src_x[0] / max_clr * 255);
                dst_x[1] = (uint8_t)((float)src_x[1] / max_clr * 255);
                dst_x[2] = (uint8_t)((float)src_x[2] / max_clr * 255);
                dst_x[3] = (uint8_t)((float)src_x[3] / max_clr * 255);
            }
        }
    }
    cairo_surface_mark_dirty(img->surface);

done:
    if (decoder) {
        avifRGBImageFreePixels(&rgb);
        avifDecoderDestroy(decoder);
    }

    return img;
}
