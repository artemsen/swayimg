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
        if (width == 0 || height == 0) {
            return nullptr;
        }

        // search for data start
        size_t pos = ss.tellg();
        while (pos < data.size && data.data[pos] != '{') {
            ++pos;
        }

        // read bitmap data
        std::vector<uint8_t> bitmap;
        bitmap.reserve(((width + 7) / 8) * height);
        while (pos < data.size) {
            while (pos < data.size && data.data[pos] != '0') {
                ++pos;
            }
            if (pos >= data.size) {
                break;
            }
            const char* num = reinterpret_cast<const char*>(data.data + pos);
            bitmap.push_back(std::strtoul(num, nullptr, 16));
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
            return std::strtoul(line.data() + pos + type.length(), nullptr, 0);
        }
        return 0;
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
