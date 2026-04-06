// SPDX-License-Identifier: MIT
// PNM image format.
// Copyright (C) 2023 Abe Wieland <abe.wieland@gmail.com>

#include "../imageformat.hpp"

#include <format>
#include <limits>
#include <utility>

class ImageFormatPnm : public ImageFormat {
public:
    ImageFormatPnm()
        : ImageFormat(Priority::Low, "pnm")
    {
    }

    ImagePtr decode(const Data& data) override
    {
        // check signature: PNM always starts with "P"
        if (data.size < 3 || data.data[0] != 'P') {
            return nullptr;
        }

        // get pnm type
        enum Type type;
        const char* type_name;
        switch (data.data[1]) {
            case '1':
            case '4':
                type = pnm_pbm;
                type_name = "PBM";
                break;
            case '2':
            case '5':
                type = pnm_pgm;
                type_name = "PGM";
                break;
            case '3':
            case '6':
                type = pnm_ppm;
                type_name = "PPM";
                break;
            default:
                return nullptr;
        }
        const bool is_ascii = data.data[1] <= '3';

        PnmIterator it;
        it.pos = data.data + 2;
        it.end = data.data + data.size;

        const int width = pnm_readint(&it, 0);
        if (width < 0) {
            return nullptr;
        }
        const int height = pnm_readint(&it, 0);
        if (height < 0) {
            return nullptr;
        }

        int maxval;
        if (type == pnm_pbm) {
            maxval = 1;
        } else {
            maxval = pnm_readint(&it, 0);
            if (maxval < 0) {
                return nullptr;
            }
            if (!maxval ||
                std::cmp_greater(maxval,
                                 std::numeric_limits<uint16_t>::max())) {
                return nullptr;
            }
        }

        if (!is_ascii) {
            // Again, the specifications technically allow for comments here,
            // but no other parsers support that (they treat that comment as
            // image data), so we won't allow one either
            const char c = *it.pos;
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                return nullptr;
            }
            ++it.pos;
        }

        // allocate image and frame
        ImagePtr image = std::make_shared<Image>();
        image->frames.resize(1);
        Pixmap& pm = image->frames[0].pm;
        pm.create(Pixmap::RGB, width, height);

        const int ret = is_ascii ? decode_ascii(pm, &it, type, maxval)
                                 : decode_raw(pm, &it, type, maxval);
        if (ret < 0) {
            return nullptr;
        }

        // set format description
        image->format =
            std::format("{} ({})", type_name, is_ascii ? "ASCII" : "raw");

        return image;
    }

private:
    // Error conditions
    static constexpr const int PNM_EEOF = -1;
    static constexpr const int PNM_ERNG = -2;
    static constexpr const int PNM_EFMT = -3;
    static constexpr const int PNM_EOVF = -4;

    // Maximum digits in INT_MAX
    static constexpr const uint8_t MAX_INT_DIGITS = 10;

    // Both assume positive arguments and evaluate b more than once
    // Divide, rounding to nearest (up on ties)
    [[nodiscard]] inline int div_near(const int a, const int b) const
    {
        return (a + b / 2) / b;
    }

    // Divide, rounding up
    [[nodiscard]] inline int div_ceil(const int a, const int b) const
    {
        return (a + b - 1) / b;
    }

    // PNM file types
    enum Type : uint8_t {
        pnm_pbm, // Bitmap
        pnm_pgm, // Grayscale pixmap
        pnm_ppm  // Color pixmap
    };

    // A file-like abstraction for cleaner number parsing
    struct PnmIterator {
        const uint8_t* pos;
        const uint8_t* end;
    };

    /**
     * Read an integer, ignoring leading whitespace and comments
     * @param it image iterator
     * @param digits maximum number of digits to read, or 0 for no limit
     * @return the integer read (positive) or an error code (negative)
     *
     * Although the specification states comments may also appear in integers,
     * this is not supported by any known parsers at the time of writing; thus,
     * we don't support it either
     */
    int pnm_readint(PnmIterator* it, size_t digits) const
    {
        if (!digits) {
            digits = MAX_INT_DIGITS;
        }
        while (it->pos != it->end) {
            const char c = *it->pos;
            if (c == '#') {
                while (it->pos != it->end && *it->pos != '\n' &&
                       *it->pos != '\r') {
                    ++it->pos;
                }
            } else if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                break;
            }
            ++it->pos;
        }

        if (it->pos == it->end) {
            return PNM_EEOF;
        }

        if (*it->pos < '0' || *it->pos > '9') {
            return PNM_EFMT;
        }

        int val = 0;
        size_t i = 0;
        do {
            const uint8_t digit = *it->pos - '0';
            if (val > std::numeric_limits<int>::max() / 10) {
                return PNM_ERNG;
            }
            val *= 10;
            if (val > std::numeric_limits<int>::max() - digit) {
                return PNM_ERNG;
            }
            val += digit;
            ++it->pos;
            ++i;
        } while (it->pos != it->end && *it->pos >= '0' && *it->pos <= '9' &&
                 i < digits);
        return val;
    }

    /**
     * Decode a plain/ASCII PNM file.
     * @param pm pixel map to write data to
     * @param it image iterator
     * @param type type of PNM file
     * @param maxval maximum value for each sample
     * @return 0 on success, error code on failure
     */
    int decode_ascii(Pixmap& pm, PnmIterator* it, enum Type type,
                     int maxval) const
    {
        for (size_t y = 0; y < pm.height(); ++y) {
            for (size_t x = 0; x < pm.width(); ++x) {
                argb_t& dst = pm.at(x, y);
                dst.a = argb_t::max;

                if (type == pnm_pbm) {
                    const int num = pnm_readint(it, 1);
                    if (num < 0) {
                        return num;
                    }
                    if (num > maxval) {
                        return PNM_EOVF;
                    }
                    dst = static_cast<uint32_t>(num - 1) | 0xff000000;
                } else if (type == pnm_pgm) {
                    int v = pnm_readint(it, 0);
                    if (v < 0) {
                        return v;
                    }
                    if (v > maxval) {
                        return PNM_EOVF;
                    }
                    if (std::cmp_not_equal(
                            maxval, std::numeric_limits<uint8_t>::max())) {
                        v = div_near(v * std::numeric_limits<uint8_t>::max(),
                                     maxval);
                    }
                    dst.r = v;
                    dst.g = v;
                    dst.b = v;
                } else { // type == pnm_ppm
                    int r = pnm_readint(it, 0);
                    if (r < 0) {
                        return r;
                    }
                    if (r > maxval) {
                        return PNM_EOVF;
                    }
                    int g = pnm_readint(it, 0);
                    if (g < 0) {
                        return g;
                    }
                    if (g > maxval) {
                        return PNM_EOVF;
                    }
                    int b = pnm_readint(it, 0);
                    if (b < 0) {
                        return b;
                    }
                    if (b > maxval) {
                        return PNM_EOVF;
                    }
                    if (std::cmp_not_equal(
                            maxval, std::numeric_limits<uint8_t>::max())) {
                        r = div_near(r * std::numeric_limits<uint8_t>::max(),
                                     maxval);
                        g = div_near(g * std::numeric_limits<uint8_t>::max(),
                                     maxval);
                        b = div_near(b * std::numeric_limits<uint8_t>::max(),
                                     maxval);
                    }
                    dst.r = r;
                    dst.g = g;
                    dst.b = b;
                }
            }
        }
        return 0;
    }

    /**
     * Decode a raw/binary PNM file.
     * @param pm image frame to write data to
     * @param it image iterator
     * @param type type of PNM file
     * @param maxval maximum value for each sample
     * @return 0 on success, error code on failure
     */
    int decode_raw(Pixmap& pm, PnmIterator* it, enum Type type,
                   int maxval) const
    {
        // Determine bytes per channel based on max value
        const size_t bytes_per_channel =
            std::cmp_less_equal(maxval, std::numeric_limits<uint8_t>::max())
            ? 1
            : 2;
        const size_t channels = (type == pnm_pgm) ? 1 : 3;

        // Calculate row size for binary format
        size_t row_size;
        if (type == pnm_pbm) {
            // PBM pads each row to the nearest whole byte
            row_size = div_ceil(pm.width(), 8);
        } else {
            row_size = pm.width() * bytes_per_channel * channels;
        }

        // Verify sufficient data
        if (it->end < it->pos + pm.height() * row_size) {
            return PNM_EEOF;
        }

        for (size_t y = 0; y < pm.height(); ++y) {
            const uint8_t* src = it->pos + y * row_size;
            for (size_t x = 0; x < pm.width(); ++x) {
                argb_t& dst = pm.at(x, y);
                dst.a = argb_t::max;

                if (type == pnm_pbm) {
                    const int bit = (src[x / 8] >> (7 - x % 8)) & 1;
                    dst =
                        static_cast<uint32_t>(bit == 0 ? 0 : 0xFF) | 0xFF000000;
                } else if (type == pnm_pgm) {
                    // Read grayscale value
                    int v;
                    if (bytes_per_channel == 1) {
                        v = src[x];
                    } else {
                        v = src[x * 2] << 8 | src[x * 2 + 1];
                    }
                    if (v > maxval) {
                        return PNM_EOVF;
                    }
                    if (std::cmp_not_equal(
                            maxval, std::numeric_limits<uint8_t>::max())) {
                        v = div_near(v * std::numeric_limits<uint8_t>::max(),
                                     maxval);
                    }
                    dst.r = v;
                    dst.g = v;
                    dst.b = v;
                } else { // type == pnm_ppm
                    int r, g, b;
                    if (bytes_per_channel == 1) {
                        r = src[x * 3];
                        g = src[x * 3 + 1];
                        b = src[x * 3 + 2];
                    } else {
                        r = src[x * 3] << 8 | src[x * 3 + 1];
                        g = src[x * 3 + 2] << 8 | src[x * 3 + 3];
                        b = src[x * 3 + 4] << 8 | src[x * 3 + 5];
                    }
                    if (r > maxval || g > maxval || b > maxval) {
                        return PNM_EOVF;
                    }
                    if (std::cmp_not_equal(
                            maxval, std::numeric_limits<uint8_t>::max())) {
                        r = div_near(r * std::numeric_limits<uint8_t>::max(),
                                     maxval);
                        g = div_near(g * std::numeric_limits<uint8_t>::max(),
                                     maxval);
                        b = div_near(b * std::numeric_limits<uint8_t>::max(),
                                     maxval);
                    }
                    dst.r = r;
                    dst.g = g;
                    dst.b = b;
                }
            }
        }
        return 0;
    }
};

// register format in factory
static ImageFormatPnm format_pnm;
