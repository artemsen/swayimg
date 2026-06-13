// SPDX-License-Identifier: MIT
// QOI image format.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "../imageformat.hpp"

#include <arpa/inet.h>

#include <cstring>
#include <format>

class ImageFormatQoi : public ImageFormat {
public:
    ImageFormatQoi() noexcept
        : ImageFormat(Priority::Low, "qoi")
    {
    }

    [[nodiscard]] ImagePtr decode(const Data& data) const override
    {
        if (!is_qoi(data)) {
            return nullptr;
        }

        const Header* qoi = reinterpret_cast<const Header*>(data.data);

        // allocate image and frame
        ImagePtr image = std::make_shared<Image>();
        image->frames.resize(1);
        Pixmap& pm = image->frames[0].pm;
        pm.create(qoi->channels == 4 ? Pixmap::ARGB : Pixmap::RGB,
                  htonl(qoi->width), htonl(qoi->height));

        // initialize decoder state
        argb_t color_map[QOI_CLRMAP_SIZE] = {};
        argb_t pixel { argb_t::max, 0, 0, 0 };
        size_t rlen = 0;
        size_t pos = sizeof(struct Header);

        // decode image
        pm.foreach([&](argb_t& px) {
            if (rlen > 0) {
                --rlen;
                px = pixel;
            } else if (pos < data.size) {
                const uint8_t tag = data.data[pos++];
                if (tag == QOI_OP_RGB) {
                    if (pos + 3 >= data.size) {
                        return;
                    }
                    pixel.r = data.data[pos++];
                    pixel.g = data.data[pos++];
                    pixel.b = data.data[pos++];
                } else if (tag == QOI_OP_RGBA) {
                    if (pos + 4 >= data.size) {
                        return;
                    }
                    pixel.r = data.data[pos++];
                    pixel.g = data.data[pos++];
                    pixel.b = data.data[pos++];
                    pixel.a = data.data[pos++];
                } else if ((tag & QOI_MASK_2) == QOI_OP_INDEX) {
                    pixel = color_map[tag & 0x3f];
                } else if ((tag & QOI_MASK_2) == QOI_OP_DIFF) {
                    pixel.r += static_cast<int8_t>((tag >> 4) & 3) - 2;
                    pixel.g += static_cast<int8_t>((tag >> 2) & 3) - 2;
                    pixel.b += static_cast<int8_t>(tag & 3) - 2;
                } else if ((tag & QOI_MASK_2) == QOI_OP_LUMA) {
                    if (pos + 1 >= data.size) {
                        return;
                    }
                    const uint8_t diff = data.data[pos++];
                    const int8_t diff_green =
                        static_cast<int8_t>(tag & 0x3f) - 32;
                    pixel.r += diff_green - 8 + ((diff >> 4) & 0x0f);
                    pixel.g += diff_green;
                    pixel.b += diff_green - 8 + (diff & 0x0f);
                } else if ((tag & QOI_MASK_2) == QOI_OP_RUN) {
                    rlen = (tag & 0x3f);
                }
                color_map[colormap_index(pixel)] = pixel;
                px = pixel;
            }
        });

        image->format = std::format("QOI {}bpp", qoi->channels * 8);

        return image;
    }

private:
    // QOI file header
    struct __attribute__((__packed__)) Header {
        uint8_t magic[4];   // Magic bytes "qoif"
        uint32_t width;     // Image width in pixels
        uint32_t height;    // Image height in pixels
        uint8_t channels;   // Number of color channels: 3 = RGB, 4 = RGBA
        uint8_t colorspace; // 0: sRGB with linear alpha, 1: all channels linear
    };

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

    /**
     * Check if format is QOI.
     * @param data source data buffer
     * @return true if it is QOI format
     */
    static bool is_qoi(const Data& data)
    {
        if (data.size <= sizeof(Header)) {
            return false;
        }
        const Header* hdr = reinterpret_cast<const Header*>(data.data);
        return std::memcmp(hdr->magic, "qoif", sizeof(hdr->magic)) == 0 &&
            hdr->width != 0 && hdr->height != 0 &&
            (hdr->channels == 3 || hdr->channels == 4);
    }
};

// register format in factory
static ImageFormatQoi format_qoi;
