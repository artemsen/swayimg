// SPDX-License-Identifier: MIT
// JPEG XL format decoder.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include <jxl/decode.h>
#include <stdlib.h>

// JPEG XL loader implementation
enum image_status decode_jxl(struct image* img, const uint8_t* data,
                             size_t size)
{
    JxlDecoder* jxl;
    JxlBasicInfo info = { 0 };
    JxlDecoderStatus status;
    size_t buffer_sz;
    struct image_frame* frames;
    size_t frame_num = 0;

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
        goto fail;
    }

    // process decoding
    status = JxlDecoderSubscribeEvents(
        jxl, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE);
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
                // convert ABGR -> ARGB
                for (size_t i = 0; i < img->frames[frame_num].pm.width *
                         img->frames[frame_num].pm.height;
                     ++i) {
                    img->frames[frame_num].pm.data[i] =
                        ABGR_TO_ARGB(img->frames[frame_num].pm.data[i]);
                }
                frame_num = img->num_frames;
                break;
            case JXL_DEC_FRAME:
                frames = realloc(img->frames,
                                 sizeof(*img->frames) * (img->num_frames + 1));
                if (!frames) {
                    goto fail;
                }
                img->frames = frames;
                if (!pixmap_create(&img->frames[frame_num].pm, info.xsize,
                                   info.ysize)) {
                    goto fail;
                }
                img->num_frames += 1;

                if (info.have_animation) {
                    JxlFrameHeader header;
                    rc = JxlDecoderGetFrameHeader(jxl, &header);
                    if (rc != JXL_DEC_SUCCESS) {
                        goto fail;
                    }
                    img->frames[frame_num].duration = header.duration *
                        1000.0f * info.animation.tps_denominator /
                        info.animation.tps_numerator;
                }
                break;
            case JXL_DEC_NEED_IMAGE_OUT_BUFFER:
                // get image buffer size
                rc = JxlDecoderImageOutBufferSize(jxl, &jxl_format, &buffer_sz);
                if (rc != JXL_DEC_SUCCESS) {
                    goto fail;
                }
                // check buffer format
                if (buffer_sz !=
                    img->frames[frame_num].pm.width *
                        img->frames[frame_num].pm.height * sizeof(argb_t)) {
                    goto fail;
                }
                // set output buffer
                rc = JxlDecoderSetImageOutBuffer(jxl, &jxl_format,
                                                 img->frames[frame_num].pm.data,
                                                 buffer_sz);
                if (rc != JXL_DEC_SUCCESS) {
                    goto fail;
                }
                break;
            default:
                break;
        }
    } while (status != JXL_DEC_SUCCESS);

    if (!img->frames) {
        goto fail;
    }

    image_set_format(img, "JPEG XL %ubpp",
                     info.bits_per_sample * info.num_color_channels +
                         info.alpha_bits);
    img->alpha = info.alpha_bits != 0;

    JxlDecoderDestroy(jxl);
    return imgload_success;

fail:
    JxlDecoderDestroy(jxl);
    image_free(img, IMGFREE_FRAMES);
    return imgload_fmterror;
}
