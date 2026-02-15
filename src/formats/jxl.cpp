// SPDX-License-Identifier: MIT
// JPEG XL format decoder.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "../imageloader.hpp"

#include <jxl/decode_cxx.h>
#include <jxl/resizable_parallel_runner_cxx.h>

#include <format>

// register format in factory
class ImageJxl;
static const ImageLoader::Registrator<ImageJxl>
    image_format_registartion("JXL", ImageLoader::Priority::High);

/* JPEG XL image. */
class ImageJxl : public Image {
public:
    bool load(const std::vector<uint8_t>& data) override
    {
        // check signature
        switch (JxlSignatureCheck(data.data(), data.size())) {
            case JXL_SIG_NOT_ENOUGH_BYTES:
            case JXL_SIG_INVALID:
                return false;
            default:
                break;
        }

        JxlDecoderPtr jxl_dec = JxlDecoderMake(nullptr);
        if (!jxl_dec) {
            return false;
        }

        JxlResizableParallelRunnerPtr jxl_prl =
            JxlResizableParallelRunnerMake(nullptr);

        JxlDecoderSubscribeEvents(jxl_dec.get(),
                                  JXL_DEC_BASIC_INFO | JXL_DEC_FRAME |
                                      JXL_DEC_FULL_IMAGE);
        JxlDecoderSetParallelRunner(jxl_dec.get(), JxlResizableParallelRunner,
                                    jxl_prl.get());

        JxlDecoderSetInput(jxl_dec.get(), data.data(), data.size());
        JxlDecoderCloseInput(jxl_dec.get());

        size_t frame_index = std::numeric_limits<size_t>::max();

        JxlBasicInfo jxl_inf {};
        const JxlPixelFormat jxl_fmt { .num_channels = 4, // ARBG
                                       .data_type = JXL_TYPE_UINT8,
                                       .endianness = JXL_NATIVE_ENDIAN,
                                       .align = 0 };
        while (true) {
            JxlDecoderStatus status = JxlDecoderProcessInput(jxl_dec.get());
            if (status == JXL_DEC_ERROR) {
                return false;
            } else if (status == JXL_DEC_NEED_MORE_INPUT) {
                return false;
            } else if (status == JXL_DEC_BASIC_INFO) {
                if (JxlDecoderGetBasicInfo(jxl_dec.get(), &jxl_inf) !=
                    JXL_DEC_SUCCESS) {
                    return false;
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
                    return false;
                }
                Pixmap& pm = frames[frame_index].pm;
                if (buffer_size != pm.stride() * pm.height()) {
                    return false;
                }
                if (JxlDecoderSetImageOutBuffer(jxl_dec.get(), &jxl_fmt,
                                                &pm.at(0, 0), buffer_size) !=
                    JXL_DEC_SUCCESS) {
                    return false;
                }
            } else if (status == JXL_DEC_FRAME) {
                // allocate new frame
                if (frame_index == std::numeric_limits<size_t>::max()) {
                    frame_index = 0; // first frame
                } else {
                    ++frame_index;
                }
                frames.resize(frame_index + 1);
                Frame& frame = frames[frame_index];
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
                frames[frame_index].pm.abgr_to_argb();
            } else if (status == JXL_DEC_SUCCESS) {
                break; // finally!
            } else {
                return false; // unknown status
            }
        }

        format =
            std::format("JPEG XL {}bpp",
                        jxl_inf.bits_per_sample * jxl_inf.num_color_channels +
                            jxl_inf.alpha_bits);
        return true;
    }
};
