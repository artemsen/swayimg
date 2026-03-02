// SPDX-License-Identifier: MIT
// Raw camera format decoder.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "../imageloader.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <libraw.h>
#pragma GCC diagnostic pop

#include <cstring>
#include <memory>

// register format in factory
class ImageRaw;
static const ImageLoader::Registrator<ImageRaw>
    image_format_registartion("RAW", ImageLoader::Priority::Normal);

/* Raw image. */
class ImageRaw : public Image {
public:
    bool load(const std::vector<uint8_t>& data) override
    {
        // open decoder
        LibRaw decoder(0);
        if (decoder.open_buffer(data.data(), data.size()) != LIBRAW_SUCCESS ||
            decoder.unpack() != LIBRAW_SUCCESS) {
            return false;
        }
        decoder.output_params_ptr()->output_bps = 8; // 8-bit color
        if (decoder.dcraw_process() != LIBRAW_SUCCESS) {
            return false;
        }

        // decode image
        using RawImage = std::unique_ptr<libraw_processed_image_t,
                                         decltype(&libraw_dcraw_clear_mem)>;
        RawImage img(decoder.dcraw_make_mem_image(), &libraw_dcraw_clear_mem);
        if (img->type != LIBRAW_IMAGE_BITMAP || img->colors != 3 ||
            img->bits != 8) {
            return false;
        }

        // copy data to image frame
        frames.resize(1);
        Pixmap& pm = frames[0].pm;
        pm.create(Pixmap::RGB, img->width, img->height);
        for (size_t y = 0; y < pm.height(); ++y) {
            for (size_t x = 0; x < pm.width(); ++x) {
                const uint8_t* src = img->data + (y * img->width + x) * 3;
                argb_t& dst = pm.at(x, y);
                dst.a = argb_t::max;
                dst.r = src[0];
                dst.g = src[1];
                dst.b = src[2];
            }
        }

        format = "RAW";

        return true;
    }
};
