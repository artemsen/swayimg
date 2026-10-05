// SPDX-License-Identifier: MIT
// Farbfeld image format.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "../imageformat.hpp"

#include <arpa/inet.h>

#include <cstring>

namespace {

class ImageFormatFarbfeld : public ImageFormat {
public:
    ImageFormatFarbfeld() noexcept
        : ImageFormat(Priority::Low, "farbfeld")
    {
    }

    [[nodiscard]] ImagePtr decode(const Data& data) const override
    {
        if (!check_signature(data,
                             { 'f', 'a', 'r', 'b', 'f', 'e', 'l', 'd' })) {
            return nullptr;
        }

        if (data.size < sizeof(Header)) {
            return nullptr;
        }
        const Header* header = reinterpret_cast<const Header*>(data.data);

        // check for data enough, size limit first: the product of two
        // 32-bit sides and pixel size can overflow 64-bit size_t
        const size_t width = htonl(header->width);
        const size_t height = htonl(header->height);
        if (width == 0 || height == 0 || width > MAX_SIZE ||
            height > MAX_SIZE ||
            (data.size - sizeof(Header)) / sizeof(Pixel) < width * height) {
            return nullptr;
        }

        // allocate image and frame
        ImagePtr image = std::make_shared<Image>();
        image->frames.resize(1);
        Pixmap& pm = image->frames[0].pm;
        pm.create(Pixmap::ARGB, width, height);

        // decode image
        const Pixel* src =
            reinterpret_cast<const Pixel*>(data.data + sizeof(Header));
        pm.foreach([&src](argb_t& pixel) {
            pixel.a = src->a;
            pixel.r = src->r;
            pixel.g = src->g;
            pixel.b = src->b;
            ++src;
        });

        image->format = "Farbfeld";

        return image;
    }

private:
    // Max size of image
    static constexpr const size_t MAX_SIZE = 65536;

    // Farbfeld file header
    struct __attribute__((__packed__)) Header {
        uint8_t magic[8];
        uint32_t width;
        uint32_t height;
    };

    // Packed Farbfeld pixel
    struct __attribute__((__packed__)) Pixel {
        uint16_t r;
        uint16_t g;
        uint16_t b;
        uint16_t a;
    };
};

// register format in factory
ImageFormatFarbfeld format_farbfeld;

} // anonymous namespace
