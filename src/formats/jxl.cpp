// SPDX-License-Identifier: MIT
// JPEG XL image format.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "../imageformat.hpp"

#include <jxl/decode_cxx.h>
#include <jxl/resizable_parallel_runner_cxx.h>

#include <format>

class ImageFormatJxl : public ImageFormat {
public:
    ImageFormatJxl()
        : ImageFormat(Priority::High, "jxl")
    {
    }

    ImagePtr decode(const Data& data) override
    {
        // check signature
        switch (JxlSignatureCheck(data.data, data.size)) {
            case JXL_SIG_NOT_ENOUGH_BYTES:
            case JXL_SIG_INVALID:
                return nullptr;
            default:
                break;
        }

        const JxlDecoderPtr jxl_dec = JxlDecoderMake(nullptr);
        if (!jxl_dec) {
            return nullptr;
        }

        ImagePtr image = std::make_shared<Image>();

        const JxlResizableParallelRunnerPtr jxl_prl =
            JxlResizableParallelRunnerMake(nullptr);

        JxlDecoderSubscribeEvents(jxl_dec.get(),
                                  JXL_DEC_BASIC_INFO | JXL_DEC_FRAME |
                                      JXL_DEC_FULL_IMAGE);
        JxlDecoderSetParallelRunner(jxl_dec.get(), JxlResizableParallelRunner,
                                    jxl_prl.get());

        JxlDecoderSetInput(jxl_dec.get(), data.data, data.size);
        JxlDecoderCloseInput(jxl_dec.get());

        size_t frame_index = std::numeric_limits<size_t>::max();

        JxlBasicInfo jxl_inf {};
        const JxlPixelFormat jxl_fmt { .num_channels = 4, // ARBG
                                       .data_type = JXL_TYPE_UINT8,
                                       .endianness = JXL_NATIVE_ENDIAN,
                                       .align = 0 };
        while (true) {
            const JxlDecoderStatus status =
                JxlDecoderProcessInput(jxl_dec.get());
            if (status == JXL_DEC_ERROR) {
                return nullptr;
            } else if (status == JXL_DEC_NEED_MORE_INPUT) {
                return nullptr;
            } else if (status == JXL_DEC_BASIC_INFO) {
                if (JxlDecoderGetBasicInfo(jxl_dec.get(), &jxl_inf) !=
                    JXL_DEC_SUCCESS) {
                    return nullptr;
                }
                JxlResizableParallelRunnerSetThreads(
                    jxl_prl.get(),
                    JxlResizableParallelRunnerSuggestThreads(jxl_inf.xsize,
                                                             jxl_inf.ysize));
            } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
                size_t buffer_size;
                if (JxlDecoderImageOutBufferSize(jxl_dec.get(), &jxl_fmt,
                                                 &buffer_size) !=
                    JXL_DEC_SUCCESS) {
                    return nullptr;
                }
                Pixmap& pm = image->frames[frame_index].pm;
                if (buffer_size != pm.stride() * pm.height()) {
                    return nullptr;
                }
                if (JxlDecoderSetImageOutBuffer(jxl_dec.get(), &jxl_fmt,
                                                &pm.at(0, 0), buffer_size) !=
                    JXL_DEC_SUCCESS) {
                    return nullptr;
                }
            } else if (status == JXL_DEC_FRAME) {
                // allocate new frame
                if (frame_index == std::numeric_limits<size_t>::max()) {
                    frame_index = 0; // first frame
                } else {
                    ++frame_index;
                }
                image->frames.resize(frame_index + 1);
                Image::Frame& frame = image->frames[frame_index];
                frame.pm.create(jxl_inf.alpha_bits ? Pixmap::ARGB : Pixmap::RGB,
                                jxl_inf.xsize, jxl_inf.ysize);
                // calculate frame timing
                if (jxl_inf.have_animation) {
                    JxlFrameHeader jxl_hdr;
                    if (JxlDecoderGetFrameHeader(jxl_dec.get(), &jxl_hdr) ==
                        JXL_DEC_SUCCESS) {
                        frame.duration = jxl_hdr.duration * 1000 *
                            jxl_inf.animation.tps_denominator /
                            jxl_inf.animation.tps_numerator;
                    }
                }
            } else if (status == JXL_DEC_FULL_IMAGE) {
                image->frames[frame_index].pm.abgr_to_argb();
            } else if (status == JXL_DEC_SUCCESS) {
                break; // finally!
            } else {
                return nullptr; // unknown status
            }
        }

        image->format =
            std::format("JPEG XL {}bpp",
                        jxl_inf.bits_per_sample * jxl_inf.num_color_channels +
                            jxl_inf.alpha_bits);
        return image;
    }
};

// register format in factory
static ImageFormatJxl format_jxl;
