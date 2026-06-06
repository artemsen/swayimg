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

    [[nodiscard]] ImagePtr decode(const Data& data) const override
    {
        // check signature
        switch (JxlSignatureCheck(data.data, data.size)) {
            case JXL_SIG_NOT_ENOUGH_BYTES:
            case JXL_SIG_INVALID:
                return nullptr;
            default:
                break;
        }

        // open and setup decoder
        const JxlDecoderPtr jxl_dec = JxlDecoderMake(nullptr);
        if (!jxl_dec) {
            return nullptr;
        }

        const JxlResizableParallelRunnerPtr jxl_prl =
            JxlResizableParallelRunnerMake(nullptr);

        JxlDecoderSubscribeEvents(jxl_dec.get(),
                                  JXL_DEC_BASIC_INFO | JXL_DEC_FRAME |
                                      JXL_DEC_FULL_IMAGE);
        JxlDecoderSetParallelRunner(jxl_dec.get(), JxlResizableParallelRunner,
                                    jxl_prl.get());

        JxlDecoderSetInput(jxl_dec.get(), data.data, data.size);
        JxlDecoderCloseInput(jxl_dec.get());

        JxlBasicInfo jxl_inf {};
        const JxlPixelFormat jxl_fmt { .num_channels = 4, // ARBG
                                       .data_type = JXL_TYPE_UINT8,
                                       .endianness = JXL_NATIVE_ENDIAN,
                                       .align = 0 };

        // docode image
        ImagePtr image = std::make_shared<Image>();
        while (true) {
            const DecodeStatus status =
                decode_step(jxl_dec, jxl_inf, jxl_fmt, jxl_prl, *image);
            if (status == DecodeStatus::Error) {
                return nullptr;
            }
            if (status == DecodeStatus::Compelete) {
                break;
            }
        }

        image->format =
            std::format("JPEG XL {}bpp",
                        jxl_inf.bits_per_sample * jxl_inf.num_color_channels +
                            jxl_inf.alpha_bits);
        return image;
    }

private:
    /** Decoder status. */
    enum class DecodeStatus : uint8_t {
        InProgress,
        Compelete,
        Error,
    };

    /**
     * Handle decode step.
     * @param jxl_dec JXL decoder instance
     * @param jxl_inf basic JXL image information
     * @param jxl_fmt JXL pixel format
     * @param jxl_prl JXL multithreaded runner
     * @param image target image instance
     * @return decode status
     */
    static DecodeStatus
    decode_step(const JxlDecoderPtr& jxl_dec, JxlBasicInfo& jxl_inf,
                const JxlPixelFormat& jxl_fmt,
                const JxlResizableParallelRunnerPtr& jxl_prl, Image& image)
    {
        const JxlDecoderStatus status = JxlDecoderProcessInput(jxl_dec.get());
        if (status == JXL_DEC_SUCCESS) {
            return DecodeStatus::Compelete;
        }
        if (status == JXL_DEC_BASIC_INFO) {
            if (JxlDecoderGetBasicInfo(jxl_dec.get(), &jxl_inf) !=
                JXL_DEC_SUCCESS) {
                return DecodeStatus::Error;
            }
            JxlResizableParallelRunnerSetThreads(
                jxl_prl.get(),
                JxlResizableParallelRunnerSuggestThreads(jxl_inf.xsize,
                                                         jxl_inf.ysize));
            return DecodeStatus::InProgress;
        }
        if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            size_t buffer_size;
            if (JxlDecoderImageOutBufferSize(jxl_dec.get(), &jxl_fmt,
                                             &buffer_size) != JXL_DEC_SUCCESS) {
                return DecodeStatus::Error;
            }
            Pixmap& pm = image.frames[image.frames.size() - 1].pm;
            if (buffer_size != pm.stride() * pm.height()) {
                return DecodeStatus::Error;
            }
            if (JxlDecoderSetImageOutBuffer(jxl_dec.get(), &jxl_fmt,
                                            &pm.at(0, 0),
                                            buffer_size) != JXL_DEC_SUCCESS) {
                return DecodeStatus::Error;
            }
            return DecodeStatus::InProgress;
        }
        if (status == JXL_DEC_FRAME) {
            // allocate new frame
            image.frames.resize(image.frames.size() + 1);
            Image::Frame& frame = image.frames[image.frames.size() - 1];
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
            return DecodeStatus::InProgress;
        }
        if (status == JXL_DEC_FULL_IMAGE) {
            image.frames[image.frames.size() - 1].pm.abgr_to_argb();
            return DecodeStatus::InProgress;
        }

        return DecodeStatus::Error;
    }
};

// register format in factory
static ImageFormatJxl format_jxl;
