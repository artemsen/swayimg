// SPDX-License-Identifier: MIT
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

//
// JPEG XL image format support
//

#include "config.h"
#ifndef HAVE_LIBJXL
#error Invalid build configuration
#endif

#include "../image.h"

#include <jxl/decode.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// JPEG XL loader implementation
struct image* load_jxl(const uint8_t* data, size_t size)
{
    struct image* img = NULL;
    uint8_t* buffer = NULL;
    size_t buffer_sz;
    JxlDecoder* jxl;
    JxlBasicInfo info;
    JxlDecoderStatus status;

    const JxlPixelFormat format = { .num_channels = 4, // ARBG
                                    .data_type = JXL_TYPE_UINT8,
                                    .endianness = JXL_NATIVE_ENDIAN,
                                    .align = 0 };

    // check signature
    switch (JxlSignatureCheck(data, size)) {
        case JXL_SIG_NOT_ENOUGH_BYTES:
        case JXL_SIG_INVALID:
            return NULL;
        default:
            break;
    }

    // initialize decoder
    jxl = JxlDecoderCreate(NULL);
    if (!jxl) {
        fprintf(stderr, "Unable to create JPEG XL decoder\n");
        return NULL;
    }
    status = JxlDecoderSetInput(jxl, data, size);
    if (status != JXL_DEC_SUCCESS) {
        fprintf(stderr, "Unable to set JPEG XL buffer [%i]\n", status);
        goto error;
    }

    // process decoding
    status =
        JxlDecoderSubscribeEvents(jxl, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE);
    if (status != JXL_DEC_SUCCESS) {
        fprintf(stderr, "JPEG XL event subscription failed\n");
        goto error;
    }
    do {
        JxlDecoderStatus rc;
        status = JxlDecoderProcessInput(jxl);
        switch (status) {
            case JXL_DEC_SUCCESS:
                break; // decoding complete
            case JXL_DEC_ERROR:
                fprintf(stderr, "JPEG XL decoder failed\n");
                goto error;
            case JXL_DEC_BASIC_INFO:
                rc = JxlDecoderGetBasicInfo(jxl, &info);
                if (rc != JXL_DEC_SUCCESS) {
                    fprintf(stderr, "Unable to get JPEG XL info [%i]\n", rc);
                    goto error;
                }
                break;
            case JXL_DEC_FULL_IMAGE:
                break; // frame decoded, nothing to do
            case JXL_DEC_NEED_IMAGE_OUT_BUFFER:
                // get image buffer size
                rc = JxlDecoderImageOutBufferSize(jxl, &format, &buffer_sz);
                if (rc != JXL_DEC_SUCCESS) {
                    fprintf(stderr, "Unable to get JPEG XL buffer size [%i]\n",
                            rc);
                    goto error;
                }
                // create cairo image
                img = create_image(CAIRO_FORMAT_ARGB32, info.xsize, info.ysize);
                if (!img) {
                    goto error;
                }
                // check buffer format
                buffer = cairo_image_surface_get_data(img->surface);
                if (buffer_sz !=
                    info.ysize * cairo_image_surface_get_stride(img->surface)) {
                    fprintf(stderr, "Unsupported JPEG XL buffer format\n");
                    goto error;
                }
                // set output buffer
                rc = JxlDecoderSetImageOutBuffer(jxl, &format, buffer,
                                                 buffer_sz);
                if (rc != JXL_DEC_SUCCESS) {
                    fprintf(stderr, "Unable to set JPEG XL buffer [%i]\n", rc);
                    goto error;
                }
                break;
            default:
                fprintf(stderr, "Unexpected JPEG XL status: %i\n", status);
        }
    } while (status != JXL_DEC_SUCCESS);

    if (!buffer) {
        // JXL_DEC_NEED_IMAGE_OUT_BUFFER was not handled, something went wrong
        fprintf(stderr, "Missed buffer initialization in JPEG XL decoder\n");
        goto error;
    }

    // convert colors: JPEG XL -> Cairo (RGBA -> ARGB)
    for (size_t i = 0; i < info.xsize * info.ysize; ++i) {
        uint8_t* pixel = buffer + i * format.num_channels;
        const uint8_t tmp = pixel[0];
        pixel[0] = pixel[2];
        pixel[2] = tmp;
    }
    cairo_surface_mark_dirty(img->surface);

    // format description: total number of bits per pixel
    set_image_meta(img, "JPEG XL %ubit",
                   info.bits_per_sample * info.num_color_channels +
                       info.alpha_bits);

    JxlDecoderDestroy(jxl);
    return img;

error:
    JxlDecoderDestroy(jxl);
    free_image(img);
    return NULL;
}
