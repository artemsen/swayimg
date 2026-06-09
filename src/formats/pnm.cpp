// SPDX-License-Identifier: MIT
// Portable anymap (PNM) image format.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "../imageformat.hpp"

#include <cctype>
#include <cstdlib>
#include <format>
#include <stdexcept>
#include <string>

class ImageFormatPnm : public ImageFormat {
public:
    ImageFormatPnm()
        : ImageFormat(Priority::Low, "pnm")
    {
    }

    [[nodiscard]] ImagePtr decode(const Data& data) const override
    {
        // check signature
        if (data.size < 3 || data.data[0] != 'P') {
            return nullptr;
        }

        // get format and encoding
        Format fmt;
        Encoding enc;
        switch (data.data[1]) {
            case '1':
                fmt = Format::BitMap;
                enc = Encoding::Plain;
                break;
            case '4':
                fmt = Format::BitMap;
                enc = Encoding::Raw;
                break;
            case '2':
                fmt = Format::GrayMap;
                enc = Encoding::Plain;
                break;
            case '5':
                fmt = Format::GrayMap;
                enc = Encoding::Raw;
                break;
            case '3':
                fmt = Format::PixMap;
                enc = Encoding::Plain;
                break;
            case '6':
                fmt = Format::PixMap;
                enc = Encoding::Raw;
                break;
            default:
                return nullptr;
        }

        try {
            size_t offset = 2; // skip 2-bytes header

            const size_t width = read_num(data, offset);
            const size_t height = read_num(data, offset);

            // bits per channel
            size_t bpc;
            if (fmt == Format::BitMap) {
                bpc = 1;
            } else {
                bpc = read_num(data, offset) <= 255 ? 1 : 2;
            }

            // skip space after number
            ++offset;
            if (offset >= data.size) {
                throw std::out_of_range("No image data");
            }

            // allocate image and frame
            ImagePtr image = std::make_shared<Image>();
            image->frames.resize(1);
            Pixmap& pm = image->frames[0].pm;
            pm.create(Pixmap::RGB, width, height);

            // decode image
            const Data px_data = { data.data + offset, data.size - offset };
            switch (fmt) {
                case Format::BitMap:
                    decode_pbm(px_data, enc, pm);
                    break;
                case Format::GrayMap:
                    decode_pgm(px_data, enc, bpc, pm);
                    break;
                case Format::PixMap:
                    decode_ppm(px_data, enc, bpc, pm);
                    break;
            }

            image->format = format_desc(fmt, enc, bpc);
            return image;

        } catch (std::out_of_range&) {
        } catch (std::invalid_argument&) {
        }
        return nullptr;
    }

private:
    /** PNM format. */
    enum class Format : uint8_t { BitMap, GrayMap, PixMap };
    /** PNM encoding. */
    enum class Encoding : uint8_t { Plain, Raw };

    /**
     * Get PNM format description.
     * @param fmt PNM format
     * @param enc PNM encoding
     * @param bpc bits per channel
     * @return text description of PNM format
     */
    static std::string format_desc(const Format fmt, const Encoding enc,
                                   const size_t bpc)
    {
        std::string desc;

        switch (fmt) {
            case Format::BitMap:
                desc = "PBM 1bit";
                break;
            case Format::GrayMap:
                desc = std::format("PGM {}bit ", bpc * 8);
                break;
            case Format::PixMap:
                desc = std::format("PPM {}bit ", bpc * 8 * 3);
                break;
        }

        desc += ' ';

        switch (fmt) {
            case Format::BitMap:
                desc += "bitmap";
                break;
            case Format::GrayMap:
                desc += "grayscale";
                break;
            case Format::PixMap:
                desc += "pixmap";
                break;
        }

        desc += ' ';

        switch (enc) {
            case Encoding::Raw:
                desc += "raw (binary)";
                break;
            case Encoding::Plain:
                desc += "plain (ASCII)";
                break;
        }

        return desc;
    }

    /**
     * Read number from data buffer.
     * @param data data buffer
     * @param offset position in the buffer
     * @return number
     * @throws std::out_of_range
     * @throws std::invalid_argument
     */
    static size_t read_num(const Data& data, size_t& offset)
    {
        // skip spaces
        while (offset < data.size && std::isspace(data.data[offset])) {
            ++offset;
        }
        if (offset >= data.size) {
            throw std::out_of_range("Out of data");
        }

        if (data.data[offset] == '#') {
            // skip comment up to the end of line
            while (offset < data.size && data.data[offset] != '\n') {
                ++offset;
            }

            // skip spaces
            while (offset < data.size && std::isspace(data.data[offset])) {
                ++offset;
            }
            if (offset >= data.size) {
                throw std::out_of_range("Out of data");
            }
        }

        // get number as text
        std::string ntext;
        while (offset < data.size && std::isdigit(data.data[offset])) {
            ntext += data.data[offset];
            ++offset;
        }
        if (ntext.empty()) {
            throw std::invalid_argument("Not a number");
        }

        return std::stoul(ntext);
    }

    /**
     * Read pixel data from ASCII data buffer.
     * @param data data buffer
     * @param bpc bits per channel
     * @return array with colors
     */
    static std::vector<argb_t::channel> read_plain(const Data& data,
                                                   const size_t bpc)
    {
        std::vector<argb_t::channel> colors;

        size_t offset = 0;
        try {
            while (offset < data.size) {
                size_t num = read_num(data, offset);
                if (bpc == 2) {
                    num >>= 8; // skip lower parts
                }
                colors.push_back(num);
            }
        } catch (std::out_of_range&) {
        } catch (std::invalid_argument&) {
        }

        return colors;
    }

    /**
     * Read pixel data from binary data buffer.
     * @param data data buffer
     * @param bpc bits per channel
     * @return array with colors
     */
    static std::vector<argb_t::channel> read_raw(const Data& data,
                                                 const size_t bpc)
    {
        std::vector<argb_t::channel> colors;

        if (bpc == 1) {
            colors.assign(data.data, data.data + data.size);
        } else {
            // skip lower parts
            colors.reserve(data.size / 2);
            for (size_t i = 0; i < data.size; i += 2) {
                colors.push_back(data.data[i]);
            }
        }

        return colors;
    }

    /**
     * Decode binary bitmap image.
     * @param data image data
     * @param pm target pixmap
     */
    static void decode_pbm_raw(const Data& data, Pixmap& pm)
    {
        size_t pos = 0;
        for (size_t y = 0; y < pm.height(); ++y) {
            for (size_t x = 0; x < pm.width(); x += 8) {
                const uint8_t bits = pos + 1 < data.size ? data.data[pos++] : 0;
                for (size_t bit = 0; bit < 8 && x + bit < pm.width(); ++bit) {
                    const bool val = (bits >> (7 - bit)) & 1;
                    argb_t& px = pm.at(x + bit, y);
                    px.a = argb_t::max;
                    px.r = val ? argb_t::min : argb_t::max;
                    px.g = px.r;
                    px.b = px.r;
                }
            }
        }
    }

    /**
     * Decode ASCII bitmap image.
     * @param data image data
     * @param pm target pixmap
     */
    static void decode_pbm_plain(const Data& data, Pixmap& pm)
    {
        size_t pos = 0;
        pm.foreach([&](argb_t& px) {
            // skip spaces
            while (pos < data.size && std::isspace(data.data[pos])) {
                ++pos;
            }
            // get color
            const argb_t::channel color =
                pos + 1 < data.size && data.data[pos++] == '0' ? argb_t::max
                                                               : argb_t::min;
            // put color
            px.a = argb_t::max;
            px.r = color;
            px.g = color;
            px.b = color;
        });
    }

    /**
     * Decode bitmap image.
     * @param data image data
     * @param enc data encoding
     * @param pm target pixmap
     */
    static void decode_pbm(const Data& data, const Encoding enc, Pixmap& pm)
    {
        if (enc == Encoding::Raw) {
            decode_pbm_raw(data, pm);
        } else {
            decode_pbm_plain(data, pm);
        }
    }

    /**
     * Decode graymap image.
     * @param data image data
     * @param enc data encoding
     * @param pm target pixmap
     */
    static void decode_pgm(const Data& data, const Encoding enc,
                           const size_t bpp, Pixmap& pm)
    {
        const std::vector<argb_t::channel> colors =
            enc == Encoding::Raw ? read_raw(data, bpp) : read_plain(data, bpp);
        size_t pos = 0;
        pm.foreach([&](argb_t& px) {
            const argb_t::channel color =
                pos + 1 < colors.size() ? colors[pos++] : 0;
            px.a = argb_t::max;
            px.r = color;
            px.g = color;
            px.b = color;
        });
    }

    /**
     * Decode pixmap image.
     * @param data image data
     * @param enc data encoding
     * @param pm target pixmap
     */
    static void decode_ppm(const Data& data, const Encoding enc,
                           const size_t bpp, Pixmap& pm)
    {
        const std::vector<argb_t::channel> colors =
            enc == Encoding::Raw ? read_raw(data, bpp) : read_plain(data, bpp);
        size_t pos = 0;
        pm.foreach([&](argb_t& px) {
            px.a = argb_t::max;
            if (pos + 2 < colors.size()) {
                px.r = colors[pos++];
                px.g = colors[pos++];
                px.b = colors[pos++];
            }
        });
    }
};

// register format in factory
static ImageFormatPnm format_pnm;
