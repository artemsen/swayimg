// SPDX-License-Identifier: MIT
// WebP image format.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "../imageformat.hpp"

#include <webp/demux.h>

#include <cstring>
#include <memory>

class ImageFormatWebp : public ImageFormat {
public:
    ImageFormatWebp()
        : ImageFormat(Priority::High, "webp")
    {
    }

    // WebP decoder wrapper
    using WebpDecoder =
        std::unique_ptr<WebPAnimDecoder, decltype(&WebPAnimDecoderDelete)>;

    ImagePtr decode(const Data& data) override
    {
        if (!check_signature(data, { 'R', 'I', 'F', 'F' })) {
            return nullptr;
        }

        // get image properties
        WebPBitstreamFeatures webp_prop;
        if (WebPGetFeatures(data.data, data.size, &webp_prop) !=
            VP8_STATUS_OK) {
            return nullptr;
        }

        // setup decoder
        WebPAnimDecoderOptions webp_opts;
        WebPAnimDecoderOptionsInit(&webp_opts);
        webp_opts.color_mode = MODE_BGRA;
        webp_opts.use_threads = true;

        // open decoder
        const WebPData webp_data = { .bytes = data.data, .size = data.size };
        const WebpDecoder webp_dec(WebPAnimDecoderNew(&webp_data, &webp_opts),
                                   &WebPAnimDecoderDelete);
        if (!webp_dec) {
            return nullptr;
        }

        WebPAnimInfo webp_info;
        if (!WebPAnimDecoderGetInfo(webp_dec.get(), &webp_info)) {
            return nullptr;
        }

        // allocate image and frames
        ImagePtr image = std::make_shared<Image>();
        image->frames.resize(webp_info.frame_count);

        // decode frames
        int prev_timestamp = 0;
        for (auto& frame : image->frames) {
            Pixmap& pm = frame.pm;
            pm.create(webp_prop.has_alpha ? Pixmap::ARGB : Pixmap::RGB,
                      webp_info.canvas_width, webp_info.canvas_height);

            uint8_t* buffer;
            int timestamp;
            if (!WebPAnimDecoderGetNext(webp_dec.get(), &buffer, &timestamp)) {
                return nullptr;
            }
            std::memcpy(pm.ptr(0, 0), buffer, pm.stride() * pm.height());

            // set frame duration
            if (webp_info.frame_count > 1) {
                if (timestamp > prev_timestamp) {
                    frame.duration = timestamp - prev_timestamp;
                } else {
                    frame.duration = 100;
                }
                prev_timestamp = timestamp;
            }
        }

        // set format description
        image->format = "WebP";
        if (webp_prop.format == 1) {
            image->format += " lossy";
        } else if (webp_prop.format == 2) {
            image->format += " lossless";
        }
        if (webp_prop.has_alpha) {
            image->format += ", alpha";
        }
        if (webp_prop.has_animation) {
            image->format += ", animation";
        }

        return image;
    }
};

// register format in factory
static ImageFormatWebp format_webp;
