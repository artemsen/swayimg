// SPDX-License-Identifier: MIT
// Sixel format decoder.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "../imageloader.hpp"

#include <sixel.h>

#include <cstdlib>

// register format in factory
class ImageSixel;
static const ImageLoader::Registrator<ImageSixel>
    image_format_registartion("Sixel", ImageLoader::Priority::Low);

/* Sixel image. */
class ImageSixel : public Image {
public:
    bool load(const std::vector<uint8_t>& data) override
    {
        // check signature: sixel always starts with Esc code
        if (data[0] != 0x1b) {
            return false;
        }

        // decode image
        SIXELSTATUS status;
        uint8_t* pixels = nullptr;
        uint8_t* palette = nullptr;
        int width = 0, height = 0, ncolors = 0;
        status = sixel_decode_raw(const_cast<uint8_t*>(data.data()),
                                  data.size(), &pixels, &width, &height,
                                  &palette, &ncolors, nullptr);
        if (SIXEL_FAILED(status) || width == 0 || height == 0) {
            std::free(pixels);
            std::free(palette);
            return false;
        }

        frames.resize(1);
        Pixmap& pm = frames[0].pm;
        pm.create(Pixmap::RGB, width, height);

        // convert palette to real pixels
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                argb_t& dst = pm.at(x, y);
                dst.a = argb_t::max;
                const uint8_t color = pixels[y * width + x];
                if (color <= ncolors) {
                    const uint8_t* rgb = &palette[color * 3];
                    dst.r = rgb[0];
                    dst.g = rgb[1];
                    dst.b = rgb[2];
                }
            }
        }

        format = "Sixel";

        std::free(pixels);
        std::free(palette);

        return true;
    }
};
