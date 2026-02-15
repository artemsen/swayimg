// SPDX-License-Identifier: MIT
// QOI format decoder.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "../imageloader.hpp"

#include <arpa/inet.h>

#include <cstring>
#include <format>

// register format in factory
class ImageQoi;
static const ImageLoader::Registrator<ImageQoi>
    image_format_registartion("QOI", ImageLoader::Priority::Low);

/* QOI image. */
class ImageQoi : public Image {
private:
    // QOI signature
    static constexpr const uint8_t signature[] = { 'q', 'o', 'i', 'f' };

    // Chunk tags
    static constexpr const uint8_t QOI_OP_INDEX = 0x00;
    static constexpr const uint8_t QOI_OP_DIFF = 0x40;
    static constexpr const uint8_t QOI_OP_LUMA = 0x80;
    static constexpr const uint8_t QOI_OP_RUN = 0xc0;
    static constexpr const uint8_t QOI_OP_RGB = 0xfe;
    static constexpr const uint8_t QOI_OP_RGBA = 0xff;

    // Mask of second byte in stream
    static constexpr const uint8_t QOI_MASK_2 = 0xc0;

    // Size of color map
    static constexpr const uint8_t QOI_CLRMAP_SIZE = 64;

    // Calc color index in map
    static constexpr uint8_t colormap_index(const argb_t clr)
    {
        return (clr.r * 3 + clr.g * 5 + clr.b * 7 + clr.a * 11) %
            QOI_CLRMAP_SIZE;
    }

    // QOI file header
    struct __attribute__((__packed__)) Header {
        uint8_t magic[4];   // Magic bytes "qoif"
        uint32_t width;     // Image width in pixels
        uint32_t height;    // Image height in pixels
        uint8_t channels;   // Number of color channels: 3 = RGB, 4 = RGBA
        uint8_t colorspace; // 0: sRGB with linear alpha, 1: all channels linear
    };

public:
    bool load(const std::vector<uint8_t>& data) override
    {
        // check signature
        if (data.size() < sizeof(Header) ||
            std::memcmp(data.data(), signature, sizeof(signature))) {
            return false;
        }

        const struct Header* qoi =
            reinterpret_cast<const struct Header*>(data.data());

        // check format
        if (qoi->width == 0 || qoi->height == 0 || qoi->channels < 3 ||
            qoi->channels > 4) {
            return false;
        }

        // allocate pixmap
        frames.resize(1);
        Pixmap& pm = frames[0].pm;
        pm.create(qoi->channels == 4 ? Pixmap::ARGB : Pixmap::RGB,
                  htonl(qoi->width), htonl(qoi->height));

        // initialize decoder state
        argb_t color_map[QOI_CLRMAP_SIZE] = {};
        argb_t pixel { argb_t::max, 0, 0, 0 };
        size_t rlen = 0;
        size_t pos = sizeof(struct Header);

        // decode image
        for (size_t y = 0; y < pm.height(); ++y) {
            for (size_t x = 0; x < pm.width(); ++x) {
                if (rlen > 0) {
                    --rlen;
                } else {
                    if (pos >= data.size()) {
                        break;
                    }
                    const uint8_t tag = data[pos++];
                    if (tag == QOI_OP_RGB) {
                        if (pos + 3 >= data.size()) {
                            return false;
                        }
                        pixel.r = data[pos++];
                        pixel.g = data[pos++];
                        pixel.b = data[pos++];
                    } else if (tag == QOI_OP_RGBA) {
                        if (pos + 4 >= data.size()) {
                            return false;
                        }
                        pixel.r = data[pos++];
                        pixel.g = data[pos++];
                        pixel.b = data[pos++];
                        pixel.a = data[pos++];
                    } else if ((tag & QOI_MASK_2) == QOI_OP_INDEX) {
                        pixel = color_map[tag & 0x3f];
                    } else if ((tag & QOI_MASK_2) == QOI_OP_DIFF) {
                        pixel.r += (int8_t)((tag >> 4) & 3) - 2;
                        pixel.g += (int8_t)((tag >> 2) & 3) - 2;
                        pixel.b += (int8_t)(tag & 3) - 2;
                    } else if ((tag & QOI_MASK_2) == QOI_OP_LUMA) {
                        uint8_t diff;
                        int8_t diff_green;
                        if (pos + 1 >= data.size()) {
                            return false;
                        }
                        diff = data[pos++];
                        diff_green = (int8_t)(tag & 0x3f) - 32;
                        pixel.r += diff_green - 8 + ((diff >> 4) & 0x0f);
                        pixel.g += diff_green;
                        pixel.b += diff_green - 8 + (diff & 0x0f);
                    } else if ((tag & QOI_MASK_2) == QOI_OP_RUN) {
                        rlen = (tag & 0x3f);
                    }
                    color_map[colormap_index(pixel)] = pixel;
                }
                pm.at(x, y) = pixel;
            }
        }

        format = std::format("QOI {}bpp", qoi->channels * 8);

        return true;
    }
};
