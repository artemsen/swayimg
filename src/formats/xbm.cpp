// SPDX-License-Identifier: MIT
// X BitMap image format.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "../imageformat.hpp"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace {

class ImageFormatXbm : public ImageFormat {
public:
    ImageFormatXbm() noexcept
        : ImageFormat(Priority::Low, "xbm")
    {
    }

    [[nodiscard]] ImagePtr decode(const Data& data) const override
    {
        // get image size
        size_t width = 0;
        size_t height = 0;
        std::stringstream ss(
            std::string(reinterpret_cast<const char*>(data.data),
                        std::min(data.size, MAX_HEADER_LEN)));
        std::string line;
        while ((width == 0 || height == 0) && std::getline(ss, line)) {
            if (width == 0) {
                width = read_size(line, "width");
            }
            if (height == 0) {
                height = read_size(line, "height");
            }
        }
        if (width == 0 || height == 0 || width > MAX_SIZE ||
            height > MAX_SIZE) {
            return nullptr;
        }

        // search for data start
        size_t pos = ss.tellg();
        while (pos < data.size && data.data[pos] != '{') {
            ++pos;
        }

        // read bitmap data
        std::vector<uint8_t> bitmap;
        bitmap.reserve(std::min(((width + 7) / 8) * height, data.size));
        while (pos < data.size) {
            while (pos < data.size && data.data[pos] != '0') {
                ++pos;
            }
            if (pos >= data.size) {
                break;
            }
            bitmap.push_back(read_hex(data, pos));
            while (pos < data.size && data.data[pos] != ',') {
                ++pos;
            }
        }

        // construct image
        ImagePtr image = std::make_shared<Image>();
        image->format = "X BitMap";
        image->frames.resize(1);
        Pixmap& pm = image->frames[0].pm;
        pm.create(Pixmap::RGB, width, height);
        fill_pixmap(bitmap, pm);

        return image;
    }

private:
    /** Max lenght of the header. */
    static constexpr const size_t MAX_HEADER_LEN = 512;

    /** Max size of the image. */
    static constexpr const size_t MAX_SIZE = 65536;

    /**
     * Read size value from text line.
     * @param line source text line
     * @param type size type ("width" or "height")
     * @return size value or 0 if value is not defined in the line
     */
    static size_t read_size(const std::string& line, const std::string& type)
    {
        const size_t pos = line.find(type);
        if (pos != std::string::npos) {
            // signed: strtoul() turns "-8" into 2^64 - 8
            const long long val =
                std::strtoll(line.data() + pos + type.length(), nullptr, 0);
            return val > 0 ? static_cast<size_t>(val) : 0;
        }
        return 0;
    }

    /**
     * Read hex number ("0x..") from the bitmap data.
     * The data buffer is not null-terminated, so strtoul() can't be used.
     * @param data source data
     * @param pos position of the number in the data buffer
     * @return the lowest byte of the number
     */
    static uint8_t read_hex(const Data& data, size_t pos)
    {
        ++pos; // skip leading '0'
        if (pos < data.size &&
            (data.data[pos] == 'x' || data.data[pos] == 'X')) {
            ++pos;
        }
        uint8_t value = 0;
        while (pos < data.size) {
            const uint8_t chr = data.data[pos++];
            uint8_t digit;
            if (chr >= '0' && chr <= '9') {
                digit = chr - '0';
            } else if (chr >= 'a' && chr <= 'f') {
                digit = chr - 'a' + 10;
            } else if (chr >= 'A' && chr <= 'F') {
                digit = chr - 'A' + 10;
            } else {
                break;
            }
            value = (value << 4) | digit;
        }
        return value;
    }

    /**
     * Fill pixmap from bitmap.
     * @param bitmap source bitmap
     * @param pm target pixmap
     */
    static void fill_pixmap(const std::vector<uint8_t>& bitmap, Pixmap& pm)
    {
        const size_t bytes_per_row = (pm.width() + 7) / 8;
        for (size_t y = 0; y < pm.height(); ++y) {
            for (size_t x = 0; x < pm.width(); ++x) {
                const size_t byte_index = (y * bytes_per_row) + (x / 8);
                if (byte_index < bitmap.size()) {
                    const size_t bit_index = x % 8;
                    const uint8_t bit = (bitmap[byte_index] >> bit_index) & 1;
                    argb_t& px = pm.at(x, y);
                    px.a = argb_t::max;
                    px.r = bit ? argb_t::min : argb_t::max;
                    px.g = px.r;
                    px.b = px.r;
                }
            }
        }
    }
};

// register format in factory
ImageFormatXbm format_xbm;

} // anonymous namespace
