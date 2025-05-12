// SPDX-License-Identifier: MIT
// JPEG XL format decoder.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include <jxl/decode.h>
#include <stdlib.h>

// JPEG XL loader implementation
enum image_status decode_jxl(struct imgdata* img, const uint8_t* data,
                             size_t size)
{
    JxlDecoder* jxl;
    JxlBasicInfo info = { 0 };
    JxlDecoderStatus status;
    struct pixmap* pm = NULL;

    const JxlPixelFormat jxl_format = { .num_channels = 4, // ARBG
                                        .data_type = JXL_TYPE_UINT8,
                                        .endianness = JXL_NATIVE_ENDIAN,
                                        .align = 0 };

    // check signature
    switch (JxlSignatureCheck(data, size)) {
        case JXL_SIG_NOT_ENOUGH_BYTES:
        case JXL_SIG_INVALID:
            return imgload_unsupported;
        default:
            break;
    }

    // initialize decoder
    jxl = JxlDecoderCreate(NULL);
    if (!jxl) {
        return imgload_fmterror;
    }
    status = JxlDecoderSetInput(jxl, data, size);
    if (status != JXL_DEC_SUCCESS) {
        goto done;
    }

    // process decoding
    status = JxlDecoderSubscribeEvents(
        jxl, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE);
    if (status != JXL_DEC_SUCCESS) {
        goto done;
    }

    if (!image_alloc_frames(img, 0)) {
        status = JXL_DEC_ERROR;
        goto done;
    }

    do {
        status = JxlDecoderProcessInput(jxl);
        switch (status) {
            case JXL_DEC_SUCCESS:
                break; // decoding complete
            case JXL_DEC_ERROR:
                goto done;
            case JXL_DEC_BASIC_INFO: {
                const JxlDecoderStatus rc = JxlDecoderGetBasicInfo(jxl, &info);
                if (rc != JXL_DEC_SUCCESS) {
                    status = rc;
                    goto done;
                }
            } break;
            case JXL_DEC_FULL_IMAGE:
                // frame loaded
                if (!pm) {
                    status = JXL_DEC_ERROR;
                    goto done;
                } else {
                    // convert ABGR to ARGB
                    const size_t pixels = pm->width * pm->height;
                    for (size_t i = 0; i < pixels; ++i) {
                        pm->data[i] = ABGR_TO_ARGB(pm->data[i]);
                    }
                    pm = NULL;
                }
                break;
            case JXL_DEC_FRAME: {
                // add new frame
                struct imgframe* frame;
                struct array* tmp = arr_append(img->frames, NULL, 1);
                if (!tmp) {
                    status = JXL_DEC_ERROR;
                    goto done;
                }
                img->frames = tmp;
                frame = arr_nth(img->frames, img->frames->size - 1);
                pm = &frame->pm;
                if (!pixmap_create(pm, info.xsize, info.ysize)) {
                    status = JXL_DEC_ERROR;
                    goto done;
                }

                if (info.have_animation) {
                    JxlFrameHeader header;
                    const JxlDecoderStatus rc =
                        JxlDecoderGetFrameHeader(jxl, &header);
                    if (rc != JXL_DEC_SUCCESS) {
                        status = rc;
                        goto done;
                    }
                    frame->duration = header.duration * 1000.0f *
                        info.animation.tps_denominator /
                        info.animation.tps_numerator;
                }
            } break;
            case JXL_DEC_NEED_IMAGE_OUT_BUFFER: {
                // get image buffer size
                size_t buffer_sz;
                JxlDecoderStatus rc;

                rc = JxlDecoderImageOutBufferSize(jxl, &jxl_format, &buffer_sz);
                if (rc != JXL_DEC_SUCCESS) {
                    status = rc;
                    goto done;
                }
                // check buffer format
                if (!pm ||
                    buffer_sz != pm->width * pm->height * sizeof(argb_t)) {
                    status = JXL_DEC_ERROR;
                    goto done;
                }
                // set output buffer
                rc = JxlDecoderSetImageOutBuffer(jxl, &jxl_format, pm->data,
                                                 buffer_sz);
                if (rc != JXL_DEC_SUCCESS) {
                    status = rc;
                    goto done;
                }
            } break;
            default:
                break;
        }
    } while (status != JXL_DEC_SUCCESS);

    if (!img->frames) {
        status = JXL_DEC_ERROR;
        goto done;
    }

    image_set_format(img, "JPEG XL %ubpp",
                     info.bits_per_sample * info.num_color_channels +
                         info.alpha_bits);
    img->alpha = info.alpha_bits != 0;

done:
    JxlDecoderDestroy(jxl);
    return status == JXL_DEC_SUCCESS ? imgload_success : imgload_fmterror;
}
