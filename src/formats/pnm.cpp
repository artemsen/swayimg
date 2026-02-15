// SPDX-License-Identifier: MIT
// PNM formats decoder
// Copyright (C) 2023 Abe Wieland <abe.wieland@gmail.com>

#include "../imageloader.hpp"

#include <format>

// register format in factory
class ImagePnm;
static const ImageLoader::Registrator<ImagePnm>
    image_format_registartion("PNM", ImageLoader::Priority::Low);

/* PNM image. */
class ImagePnm : public Image {
private:
    // Error conditions
    static constexpr const int PNM_EEOF = -1;
    static constexpr const int PNM_ERNG = -2;
    static constexpr const int PNM_EFMT = -3;
    static constexpr const int PNM_EOVF = -4;

    // Digits in INT_MAX
    static constexpr const uint8_t INT_MAX_DIGITS = 10;

    // Both assume positive arguments and evaluate b more than once
    // Divide, rounding to nearest (up on ties)
    constexpr int div_near(int a, int b) const { return ((a) + (b) / 2) / (b); }

    // Divide, rounding up
    constexpr int div_ceil(int a, int b) const { return ((a) + (b)-1) / (b); }

    // PNM file types
    enum Type : uint8_t {
        pnm_pbm, // Bitmap
        pnm_pgm, // Grayscale pixmap
        pnm_ppm  // Color pixmap
    };

    // A file-like abstraction for cleaner number parsing
    struct pnm_iter {
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
    int pnm_readint(struct pnm_iter* it, size_t digits) const
    {
        if (!digits) {
            digits = INT_MAX_DIGITS;
        }
        for (; it->pos != it->end; ++it->pos) {
            const char c = *it->pos;
            if (c == '#') {
                while (it->pos != it->end && *it->pos != '\n' &&
                       *it->pos != '\r') {
                    ++it->pos;
                }
            } else if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                break;
            }
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
            const uint8_t d = *it->pos - '0';
            if (val > INT_MAX / 10) {
                return PNM_ERNG;
            }
            val *= 10;
            if (val > INT_MAX - d) {
                return PNM_ERNG;
            }
            val += d;
            ++it->pos;
            ++i;
        } while (it->pos != it->end && *it->pos >= '0' && *it->pos <= '9' &&
                 i < digits);
        return val;
    }

    /**
     * Decode a plain/ASCII PNM file
     * @param pm pixel map to write data to
     * @param it image iterator
     * @param type type of PNM file
     * @param maxval maximum value for each sample
     * @return 0 on success, error code on failure
     */
    int decode_plain(Pixmap& pm, struct pnm_iter* it, enum Type type,
                     int maxval) const
    {
        for (size_t y = 0; y < pm.height(); ++y) {
            for (size_t x = 0; x < pm.width(); ++x) {
                argb_t& dst = pm.at(x, y);
                dst.a = argb_t::max;
                if (type == pnm_pbm) {
                    int bit = pnm_readint(it, 1);
                    if (bit < 0) {
                        return bit;
                    }
                    if (bit > maxval) {
                        return PNM_EOVF;
                    }
                    dst = static_cast<uint32_t>(bit - 1) | 0xff000000;
                } else if (type == pnm_pgm) {
                    int v = pnm_readint(it, 0);
                    if (v < 0) {
                        return v;
                    }
                    if (v > maxval) {
                        return PNM_EOVF;
                    }
                    if (maxval != UINT8_MAX) {
                        v = div_near(v * UINT8_MAX, maxval);
                    }
                    dst.r = v;
                    dst.g = v;
                    dst.b = v;
                } else {
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
                    if (maxval != UINT8_MAX) {
                        r = div_near(r * UINT8_MAX, maxval);
                        g = div_near(g * UINT8_MAX, maxval);
                        b = div_near(b * UINT8_MAX, maxval);
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
     * Decode a raw/binary PNM file
     * @param pm image frame to write data to
     * @param it image iterator
     * @param type type of PNM file
     * @param maxval maximum value for each sample
     * @return 0 on success, error code on failure
     */
    int decode_raw(Pixmap& pm, pnm_iter* it, enum Type type, int maxval) const
    {
        // PGM and PPM use bpc (bytes per channel) bytes for each channel
        // depending on the max, with 1 channel for PGM and 3 for PPM; PBM pads
        // each row to the nearest whole byte
        size_t bpc = maxval <= UINT8_MAX ? 1 : 2;
        size_t rowsz = type == pnm_pbm
            ? div_ceil(pm.width(), 8)
            : pm.width() * bpc * (type == pnm_pgm ? 1 : 3);
        if (it->end < it->pos + pm.height() * rowsz) {
            return PNM_EEOF;
        }
        for (size_t y = 0; y < pm.height(); ++y) {
            const uint8_t* src = it->pos + y * rowsz;
            for (size_t x = 0; x < pm.width(); ++x) {
                argb_t& dst = pm.at(x, y);
                dst.a = argb_t::max;
                if (type == pnm_pbm) {
                    const int bit = (src[x / 8] >> (7 - x % 8)) & 1;
                    dst = static_cast<uint32_t>(bit - 1) | 0xff000000;
                } else if (type == pnm_pgm) {
                    int v = bpc == 1 ? src[x] : src[x] << 8 | src[x + 1];
                    if (v > maxval) {
                        return PNM_EOVF;
                    }
                    if (maxval != UINT8_MAX) {
                        v = div_near(v * UINT8_MAX, maxval);
                    }
                    dst.r = v;
                    dst.g = v;
                    dst.b = v;
                } else {
                    int r, g, b;
                    if (bpc == 1) {
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
                    if (maxval != UINT8_MAX) {
                        r = div_near(r * UINT8_MAX, maxval);
                        g = div_near(g * UINT8_MAX, maxval);
                        b = div_near(b * UINT8_MAX, maxval);
                    }
                    dst.r = r;
                    dst.g = g;
                    dst.b = b;
                }
            }
        }
        return 0;
    }

public:
    bool load(const std::vector<uint8_t>& data) override
    {
        // check signature: PNM always starts with "P"
        if (data.size() < 3 || data[0] != 'P') {
            return false;
        }

        // get pnm type
        enum Type type;
        switch (data[1]) {
            case '1':
            case '4':
                type = pnm_pbm;
                break;
            case '2':
            case '5':
                type = pnm_pgm;
                break;
            case '3':
            case '6':
                type = pnm_ppm;
                break;
            default:
                return false;
        }
        const bool plain = data[1] <= '3';

        struct pnm_iter it;
        it.pos = data.data() + 2;
        it.end = data.data() + data.size();

        const int width = pnm_readint(&it, 0);
        if (width < 0) {
            return false;
        }
        const int height = pnm_readint(&it, 0);
        if (height < 0) {
            return false;
        }

        int maxval;
        if (type == pnm_pbm) {
            maxval = 1;
        } else {
            maxval = pnm_readint(&it, 0);
            if (maxval < 0) {
                return false;
            }
            if (!maxval || maxval > UINT16_MAX) {
                return false;
            }
        }
        if (!plain) {
            // Again, the specifications technically allow for comments here,
            // but no other parsers support that (they treat that comment as
            // image data), so we won't allow one either
            const char c = *it.pos;
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                return false;
            }
            ++it.pos;
        }

        // allocate pixmap
        frames.resize(1);
        Pixmap& pm = frames[0].pm;
        pm.create(Pixmap::RGB, width, height);

        const int ret = plain ? decode_plain(pm, &it, type, maxval)
                              : decode_raw(pm, &it, type, maxval);
        if (ret < 0) {
            return false;
        }

        format = std::format(
            "P{}M ({})", type == pnm_pbm ? 'B' : (type == pnm_pgm ? 'G' : 'P'),
            plain ? "ASCII" : "raw");

        return true;
    }
};
