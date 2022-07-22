// SPDX-License-Identifier: MIT
// JPEG XL format decoder.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include <jxl/decode.h>
#include <string.h>

// JPEG XL loader implementation
bool load_jxl(image_t* img, const uint8_t* data, size_t size)
{
    size_t buffer_sz;
    JxlDecoder* jxl;
    JxlBasicInfo info;
    JxlDecoderStatus status;

    const JxlPixelFormat jxl_format = { .num_channels = 4, // ARBG
                                        .data_type = JXL_TYPE_UINT8,
                                        .endianness = JXL_NATIVE_ENDIAN,
                                        .align = 0 };

    // check signature
    switch (JxlSignatureCheck(data, size)) {
        case JXL_SIG_NOT_ENOUGH_BYTES:
        case JXL_SIG_INVALID:
            return false;
        default:
            break;
    }

    // initialize decoder
    jxl = JxlDecoderCreate(NULL);
    if (!jxl) {
        image_error(img, "unable to create jpeg xl decoder");
        return false;
    }
    status = JxlDecoderSetInput(jxl, data, size);
    if (status != JXL_DEC_SUCCESS) {
        image_error(img, "unable to set jpeg xl buffer: error %d", status);
        goto fail;
    }

    // process decoding
    status =
        JxlDecoderSubscribeEvents(jxl, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE);
    if (status != JXL_DEC_SUCCESS) {
        image_error(img, "jpeg xl event subscription failed");
        goto fail;
    }
    do {
        JxlDecoderStatus rc;
        status = JxlDecoderProcessInput(jxl);
        switch (status) {
            case JXL_DEC_SUCCESS:
                break; // decoding complete
            case JXL_DEC_ERROR:
                image_error(img, "failed to decode jpeg xl");
                goto fail;
            case JXL_DEC_BASIC_INFO:
                rc = JxlDecoderGetBasicInfo(jxl, &info);
                if (rc != JXL_DEC_SUCCESS) {
                    image_error(img, "unable to get jpeg xl info: error %d",
                                rc);
                    goto fail;
                }
                break;
            case JXL_DEC_FULL_IMAGE:
                break; // frame decoded, nothing to do
            case JXL_DEC_NEED_IMAGE_OUT_BUFFER:
                // get image buffer size
                rc = JxlDecoderImageOutBufferSize(jxl, &jxl_format, &buffer_sz);
                if (rc != JXL_DEC_SUCCESS) {
                    image_error(img, "unable to get jpeg xl buffer: error %d",
                                rc);
                    goto fail;
                }
                if (!image_allocate(img, info.xsize, info.ysize)) {
                    goto fail;
                }
                // check buffer format
                if (buffer_sz !=
                    img->width * img->height * sizeof(img->data[0])) {
                    image_error(img, "unsupported jpeg xl buffer format");
                    goto fail;
                }
                // set output buffer
                rc = JxlDecoderSetImageOutBuffer(jxl, &jxl_format, img->data,
                                                 buffer_sz);
                if (rc != JXL_DEC_SUCCESS) {
                    image_error(img, "unable to set jpeg xl buffer: error %d",
                                rc);
                    goto fail;
                }
                break;
            default:
                image_error(img, "unexpected jpeg xl status %d", status);
        }
    } while (status != JXL_DEC_SUCCESS);

    // convert RGBA -> ARGB
    for (size_t i = 0; i < img->width * img->height; ++i) {
        uint32_t val = img->data[i];
        img->data[i] =
            (val & 0xff00ff00) | (val & 0xff) << 16 | ((val >> 16) & 0xff);
    }

    // format description: total number of bits per pixel
    add_image_info(img, "Format", "JPEG XL %ubit",
                   info.bits_per_sample * info.num_color_channels +
                       info.alpha_bits);
    img->alpha = true;

    JxlDecoderDestroy(jxl);
    return true;

fail:
    JxlDecoderDestroy(jxl);
    image_deallocate(img);
    return false;
}
