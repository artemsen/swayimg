// SPDX-License-Identifier: MIT
// Farbfeld format decoder.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "../imageloader.hpp"

#include <arpa/inet.h>

#include <cstring>

// register format in factory
class ImageFarbfeld;
static const ImageLoader::Registrator<ImageFarbfeld>
    image_format_registartion("Farbfeld", ImageLoader::Priority::Low);

/* Farbfeld image. */
class ImageFarbfeld : public Image {
private:
    // Farbfeld signature
    static constexpr const uint8_t signature[] = { 'f', 'a', 'r', 'b',
                                                   'f', 'e', 'l', 'd' };

    // Farbfeld file header
    struct __attribute__((__packed__)) Header {
        uint8_t magic[sizeof(signature)];
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

public:
    bool load(const std::vector<uint8_t>& data) override
    {
        const Header* header = reinterpret_cast<const Header*>(data.data());

        // check signature
        if (data.size() < sizeof(Header) ||
            std::memcmp(header->magic, signature, sizeof(signature))) {
            return false;
        }

        // check for data enough
        const size_t width = htonl(header->width);
        const size_t height = htonl(header->height);
        if (data.size() - sizeof(Header) < width * height * sizeof(Pixel)) {
            return false;
        }

        // create pixmap
        frames.resize(1);
        Pixmap& pm = frames[0].pm;
        pm.create(Pixmap::ARGB, htonl(header->width), htonl(header->height));

        // decode image
        const Pixel* src =
            reinterpret_cast<const Pixel*>(data.data() + sizeof(Header));
        pm.foreach([&src](argb_t& pixel) {
            pixel.a = src->a;
            pixel.r = src->r;
            pixel.g = src->g;
            pixel.b = src->b;
            ++src;
        });

        format = "Farbfeld";

        return true;
    }
};
