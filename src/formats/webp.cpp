// SPDX-License-Identifier: MIT
// WebP format decoder.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "../imageloader.hpp"

#include <webp/demux.h>

#include <cstring>
#include <memory>

// register format in factory
class ImageWebp;
static const ImageLoader::Registrator<ImageWebp>
    image_format_registartion("WebP", ImageLoader::Priority::High);

/* WebP image. */
class ImageWebp : public Image {
private:
    // WebP signature
    static constexpr const uint8_t signature[] = { 'R', 'I', 'F', 'F' };

    // WebP decoder wrapper
    using WebpDecoder =
        std::unique_ptr<WebPAnimDecoder, decltype(&WebPAnimDecoderDelete)>;

public:
    bool load(const std::vector<uint8_t>& data) override
    {
        // check signature
        if (data.size() < sizeof(signature) ||
            std::memcmp(data.data(), signature, sizeof(signature))) {
            return false;
        }

        // get image properties
        WebPBitstreamFeatures webp_prop;
        if (WebPGetFeatures(data.data(), data.size(), &webp_prop) !=
            VP8_STATUS_OK) {
            return false;
        }

        // setup decoder
        WebPAnimDecoderOptions webp_opts;
        WebPAnimDecoderOptionsInit(&webp_opts);
        webp_opts.color_mode = MODE_BGRA;
        webp_opts.use_threads = true;

        // open decoder
        const WebPData wepp_data = { .bytes = data.data(),
                                     .size = data.size() };
        WebpDecoder webp_dec(WebPAnimDecoderNew(&wepp_data, &webp_opts),
                             &WebPAnimDecoderDelete);
        if (!webp_dec) {
            return false;
        }

        // allocate frames
        WebPAnimInfo webp_info;
        if (!WebPAnimDecoderGetInfo(webp_dec.get(), &webp_info)) {
            return false;
        }
        frames.resize(webp_info.frame_count);

        // decode frames
        int prev_timestamp = 0;
        for (auto& it : frames) {
            Pixmap& pm = it.pm;
            pm.create(webp_prop.has_alpha ? Pixmap::ARGB : Pixmap::RGB,
                      webp_info.canvas_width, webp_info.canvas_height);

            uint8_t* buffer;
            int timestamp;
            if (!WebPAnimDecoderGetNext(webp_dec.get(), &buffer, &timestamp)) {
                return false;
            }
            std::memcpy(pm.ptr(0, 0), buffer, pm.stride() * pm.height());

            // set frame duration
            if (webp_info.frame_count > 1) {
                if (timestamp > prev_timestamp) {
                    it.duration = timestamp - prev_timestamp;
                } else {
                    it.duration = 100;
                }
                prev_timestamp = timestamp;
            }
        }

        // set format description
        format = "WebP";
        if (webp_prop.format == 1) {
            format += " lossy";
        } else if (webp_prop.format == 2) {
            format += " lossless";
        }
        if (webp_prop.has_alpha) {
            format += ", alpha";
        }
        if (webp_prop.has_animation) {
            format += ", animation";
        }

        return true;
    }
};
