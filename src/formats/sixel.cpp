// SPDX-License-Identifier: MIT
// Sixel image format.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "../imageformat.hpp"

#include <sixel.h>

#include <cstdlib>
#include <utility>

class ImageFormatSixel : public ImageFormat {
public:
    ImageFormatSixel()
        : ImageFormat(Priority::Low, "sixel")
    {
    }

    ImagePtr decode(const Data& data) override
    {
        // check signature: sixel always starts with Esc code
        if (data.data[0] != 0x1b) {
            return nullptr;
        }

        // decode image
        SIXELSTATUS status;
        uint8_t* pixels = nullptr;
        uint8_t* palette = nullptr;
        int width = 0, height = 0, ncolors = 0;
        status = sixel_decode_raw(const_cast<uint8_t*>(data.data), data.size,
                                  &pixels, &width, &height, &palette, &ncolors,
                                  nullptr);
        if (SIXEL_FAILED(status) || width == 0 || height == 0) {
            std::free(pixels);
            std::free(palette);
            return nullptr;
        }

        // allocate image and frame
        ImagePtr image = std::make_shared<Image>();
        image->frames.resize(1);
        Pixmap& pm = image->frames[0].pm;
        pm.create(Pixmap::RGB, width, height);

        // convert palette to real pixels
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                argb_t& dst = pm.at(x, y);
                dst.a = argb_t::max;
                const uint8_t color = pixels[y * width + x];
                if (std::cmp_less_equal(color, ncolors)) {
                    const uint8_t* rgb = &palette[color * 3];
                    dst.r = rgb[0];
                    dst.g = rgb[1];
                    dst.b = rgb[2];
                }
            }
        }

        image->format = "Sixel";

        std::free(pixels);
        std::free(palette);

        return image;
    }
};

// register format in factory
static ImageFormatSixel format_sixel;
