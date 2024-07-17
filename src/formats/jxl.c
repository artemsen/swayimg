// SPDX-License-Identifier: MIT
// JPEG XL format decoder.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "../loader.h"

#include <jxl/decode.h>
#include <string.h>

// JPEG XL loader implementation
enum loader_status decode_jxl(struct image* ctx, const uint8_t* data,
                              size_t size)
{
    JxlDecoder* jxl;
    JxlBasicInfo info;
    JxlDecoderStatus status;
    size_t buffer_sz;
    struct pixmap* pm = NULL;

    const JxlPixelFormat jxl_format = { .num_channels = 4, // ARBG
                                        .data_type = JXL_TYPE_UINT8,
                                        .endianness = JXL_NATIVE_ENDIAN,
                                        .align = 0 };

    // check signature
    switch (JxlSignatureCheck(data, size)) {
        case JXL_SIG_NOT_ENOUGH_BYTES:
        case JXL_SIG_INVALID:
            return ldr_unsupported;
        default:
            break;
    }

    // initialize decoder
    jxl = JxlDecoderCreate(NULL);
    if (!jxl) {
        return ldr_fmterror;
    }
    status = JxlDecoderSetInput(jxl, data, size);
    if (status != JXL_DEC_SUCCESS) {
        goto fail;
    }

    // process decoding
    status =
        JxlDecoderSubscribeEvents(jxl, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE);
    if (status != JXL_DEC_SUCCESS) {
        goto fail;
    }
    do {
        JxlDecoderStatus rc;
        status = JxlDecoderProcessInput(jxl);
        switch (status) {
            case JXL_DEC_SUCCESS:
                break; // decoding complete
            case JXL_DEC_ERROR:
                goto fail;
            case JXL_DEC_BASIC_INFO:
                rc = JxlDecoderGetBasicInfo(jxl, &info);
                if (rc != JXL_DEC_SUCCESS) {
                    goto fail;
                }
                break;
            case JXL_DEC_FULL_IMAGE:
                break; // frame decoded, nothing to do
            case JXL_DEC_NEED_IMAGE_OUT_BUFFER:
                // get image buffer size
                rc = JxlDecoderImageOutBufferSize(jxl, &jxl_format, &buffer_sz);
                if (rc != JXL_DEC_SUCCESS) {
                    goto fail;
                }
                pm = image_allocate_frame(ctx, info.xsize, info.ysize);
                if (!pm) {
                    goto fail;
                }
                // check buffer format
                if (buffer_sz != pm->width * pm->height * sizeof(argb_t)) {
                    goto fail;
                }
                // set output buffer
                rc = JxlDecoderSetImageOutBuffer(jxl, &jxl_format, pm->data,
                                                 buffer_sz);
                if (rc != JXL_DEC_SUCCESS) {
                    goto fail;
                }
                break;
            default:
                break;
        }
    } while (status != JXL_DEC_SUCCESS);

    if (!pm) {
        goto fail;
    }

    // convert ABGR -> ARGB
    for (size_t i = 0; i < pm->width * pm->height; ++i) {
        pm->data[i] = ABGR_TO_ARGB(pm->data[i]);
    }

    image_set_format(ctx, "JPEG XL %ubpp",
                     info.bits_per_sample * info.num_color_channels +
                         info.alpha_bits);
    ctx->alpha = info.alpha_bits != 0;

    JxlDecoderDestroy(jxl);
    return ldr_success;

fail:
    JxlDecoderDestroy(jxl);
    image_free_frames(ctx);
    return ldr_fmterror;
}
