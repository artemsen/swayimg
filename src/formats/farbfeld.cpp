// SPDX-License-Identifier: MIT
// Farbfeld image format.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "../imageformat.hpp"

#include <arpa/inet.h>

#include <cstring>

class ImageFormatFarbfeld : public ImageFormat {
public:
    ImageFormatFarbfeld()
        : ImageFormat(Priority::Low, "farbfeld")
    {
    }

    ImagePtr decode(const Data& data) override
    {
        if (!check_signature(data,
                             { 'f', 'a', 'r', 'b', 'f', 'e', 'l', 'd' })) {
            return nullptr;
        }

        const Header* header = reinterpret_cast<const Header*>(data.data);

        // check for data enough
        const size_t width = htonl(header->width);
        const size_t height = htonl(header->height);
        if (data.size - sizeof(Header) < width * height * sizeof(Pixel)) {
            return nullptr;
        }

        // allocate image and frame
        ImagePtr image = std::make_shared<Image>();
        image->frames.resize(1);
        Pixmap& pm = image->frames[0].pm;
        pm.create(Pixmap::ARGB, htonl(header->width), htonl(header->height));

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
static ImageFormatFarbfeld format_farbfeld;
