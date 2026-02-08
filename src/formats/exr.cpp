// SPDX-License-Identifier: MIT
// EXR format decoder.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#include "../imageloader.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <ImfArray.h>
#include <ImfInputFile.h>
#include <ImfRgbaFile.h>
#include <ImfTiledRgbaFile.h>
#pragma GCC diagnostic pop

#include <cstring>
#include <format>

// register format in factory
class ImageExr;
static const ImageLoader::Registrator<ImageExr>
    image_format_registartion("EXR", ImageLoader::Priority::Low);

/** Memory input stream. */
struct MemoryIStream : public Imf::IStream {
    MemoryIStream(const std::vector<uint8_t>& raw_data)
        : Imf::IStream("MemoryIStream")
        , data(raw_data)
        , position(0)
    {
    }

    bool read(char c[], int n) override
    {
        if (position + n > data.size()) {
            throw std::runtime_error("No more data");
        }
        std::memcpy(c, data.data() + position, n);
        position += n;
        return position < data.size();
    }

    int64_t size() override { return data.size(); }
    uint64_t tellg() override { return position; }
    void seekg(uint64_t pos) override { position = pos; }
    void clear() override { position = 0; }
    bool isMemoryMapped() const override { return false; }

private:
    const std::vector<uint8_t>& data;
    uint64_t position;
};

/* EXR image. */
class ImageExr : public Image {
private:
    // EXR signature
    static constexpr const uint8_t signature[] = { 0x76, 0x2f, 0x31, 0x01 };

public:
    bool load(const std::vector<uint8_t>& data) override
    {
        // check signature
        if (data.size() < sizeof(signature) ||
            std::memcmp(data.data(), signature, sizeof(signature))) {
            return false;
        }

        try {
            const char* image_type;
            MemoryIStream stream(data);
            Imf::InputFile exr_file(stream, 0);
            const Imf::Header& exr_header = exr_file.header();

            const Imath::Box2i& box = exr_header.dataWindow();
            const int width = box.max.x - box.min.x + 1;
            const int height = box.max.y - box.min.y + 1;
            const int dx = box.min.x;
            const int dy = box.min.y;

            Imf::Array2D<Imf::Rgba> pixels;
            pixels.resizeErase(height, width);

            // decode image
            if (exr_header.hasTileDescription()) {
                image_type = "tiled";
                Imf::TiledRgbaInputFile tile_file(stream);
                tile_file.setFrameBuffer(&pixels[-dy][-dx], 1, width);
                tile_file.readTiles(0, tile_file.numXTiles() - 1, 0,
                                    tile_file.numYTiles() - 1);
            } else {
                image_type = "scanline";
                Imf::RgbaInputFile rgba_file(stream);
                rgba_file.setFrameBuffer(&pixels[0][0] - dx - dy * width, 1,
                                         width);
                rgba_file.readPixels(box.min.y, box.max.y);
            }

            // allocate frame
            frames.resize(1);
            Pixmap& pm = frames[0].pm;
            pm.create(Pixmap::ARGB, width, height);

            // put image to pixmap
            for (size_t y = 0; y < pm.height(); ++y) {
                for (size_t x = 0; x < pm.width(); ++x) {
                    argb_t& dst = pm.at(x, y);
                    const Imf::Rgba& clr = pixels[y][x];
                    dst.a = static_cast<uint8_t>(
                        std::min(static_cast<size_t>(argb_t::max),
                                 static_cast<size_t>(clr.a * argb_t::max)));
                    dst.r = static_cast<uint8_t>(
                        std::min(static_cast<size_t>(argb_t::max),
                                 static_cast<size_t>(clr.r * argb_t::max)));
                    dst.g = static_cast<uint8_t>(
                        std::min(static_cast<size_t>(argb_t::max),
                                 static_cast<size_t>(clr.g * argb_t::max)));
                    dst.b = static_cast<uint8_t>(
                        std::min(static_cast<size_t>(argb_t::max),
                                 static_cast<size_t>(clr.b * argb_t::max)));
                }
            }

            std::string compression;
            getCompressionNameFromId(exr_header.compression(), compression);
            format = std::format("EXR ({}, {})", image_type, compression);

        } catch (const std::exception&) {
            return false;
        }

        return true;
    }
};
