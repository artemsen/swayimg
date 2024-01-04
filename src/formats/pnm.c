// SPDX-License-Identifier: MIT
// PNM formats decoder
// Copyright (C) 2023 Abe Wieland <abe.wieland@gmail.com>

#include "loader.h"

#include <limits.h>

// Divide a by b, rounding to the nearest integer; evaluates b twice
#define div_near(a, b) (((a) + (b) / 2) / (b))
// Divide a by b, rounding up; evaluates b twice
#define div_ceil(a, b) (((a) + (b)-1) / (b))

// PNM file types
enum pnm_type {
    pnm_pbm, // Bitmap
    pnm_pgm, // Grayscale pixmap
    pnm_ppm  // Color pixmap
};

// A file-like abstraction for cleaner number parsing
struct pnm_iter {
    const uint8_t* pos;
    const uint8_t* end;
};

// Error conditions
#define PNM_EEOF -1
#define PNM_ERNG -2
#define PNM_EFMT -3
#define PNM_EOVF -4

/**
 * Return string for error conditions above
 * @param err error code
 * @return string representing that error code
 */
static const char* pnm_strerror(int err)
{
    switch (err) {
        case PNM_EEOF:
            return "unexpected end of image";
        case PNM_ERNG:
            return "integer too large";
        case PNM_EFMT:
            return "digit expected";
        case PNM_EOVF:
            return "pixel value above maxval";
        default:
            return "unknown error";
    }
}

// Digits in INT_MAX
#define INT_MAX_DIGITS 10

/**
 * Read an integer, ignoring leading whitespace and comments
 * @param it image iterator
 * @param digits maximum number of digits to read, or 0 for no limit
 * @return the integer read (positive) or an error code (negative)
 *
 * Although the specification states comments may also appear in integers, this
 * is not supported by any known parsers at the time of writing; thus, we don't
 * support it either
 */
static int pnm_readint(struct pnm_iter* it, size_t digits)
{
    if (!digits)
        digits = INT_MAX_DIGITS;
    for (; it->pos != it->end; ++it->pos) {
        const char c = *it->pos;
        if (c == '#') {
            while (it->pos != it->end && *it->pos != '\n' && *it->pos != '\r')
                ++it->pos;
        } else if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            break;
        }
    }
    if (it->pos == it->end)
        return PNM_EEOF;
    if (*it->pos < '0' || *it->pos > '9')
        return PNM_EFMT;
    int val = 0;
    size_t i = 0;
    do {
        const uint8_t d = *it->pos - '0';
        if (val > INT_MAX / 10)
            return PNM_ERNG;
        val *= 10;
        if (val > INT_MAX - d)
            return PNM_ERNG;
        val += d;
        ++it->pos;
        ++i;
    } while (it->pos != it->end && *it->pos >= '0' && *it->pos <= '9' &&
             i < digits);
    return val;
}

/**
 * Decode a plain/ASCII PNM file
 * @param f image frame to write data to
 * @param it image iterator
 * @param type type of PNM file
 * @param maxval maximum value for each sample
 * @return 0 on success, error code on failure
 */
static int decode_plain(struct image_frame* f, struct pnm_iter* it,
                        enum pnm_type type, int maxval)
{
    for (size_t y = 0; y < f->height; ++y) {
        argb_t* dst = f->data + y * f->width;
        for (size_t x = 0; x < f->width; ++x) {
            argb_t pix = ARGB_SET_A(0xff);
            if (type == pnm_pbm) {
                const int bit = pnm_readint(it, 1);
                if (bit < 0)
                    return bit;
                if (bit > maxval)
                    return PNM_EOVF;
                pix |= bit - 1;
            } else if (type == pnm_pgm) {
                int v = pnm_readint(it, 0);
                if (v < 0)
                    return v;
                if (v > maxval)
                    return PNM_EOVF;
                if (maxval != UINT8_MAX)
                    v = div_near(v * UINT8_MAX, maxval);
                pix |= ARGB_SET_R(v) | ARGB_SET_G(v) | ARGB_SET_B(v);
            } else {
                int r = pnm_readint(it, 0);
                if (r < 0)
                    return r;
                if (r > maxval)
                    return PNM_EOVF;
                int g = pnm_readint(it, 0);
                if (g < 0)
                    return g;
                if (g > maxval)
                    return PNM_EOVF;
                int b = pnm_readint(it, 0);
                if (b < 0)
                    return b;
                if (b > maxval)
                    return PNM_EOVF;
                if (maxval != UINT8_MAX) {
                    r = div_near(r * UINT8_MAX, maxval);
                    g = div_near(g * UINT8_MAX, maxval);
                    b = div_near(b * UINT8_MAX, maxval);
                }
                pix |= ARGB_SET_R(r) | ARGB_SET_G(g) | ARGB_SET_B(b);
            }
            dst[x] = pix;
        }
    }
    return 0;
}

/**
 * Decode a raw/binary PNM file
 * @param f image frame to write data to
 * @param it image iterator
 * @param type type of PNM file
 * @param maxval maximum value for each sample
 * @return 0 on success, error code on failure
 */
static int decode_raw(struct image_frame* f, struct pnm_iter* it,
                      enum pnm_type type, int maxval)
{
    // PGM and PPM use bpc (bytes per channel) bytes for each channel depending
    // on the max, with 1 channel for PGM and 3 for PPM; PBM pads each row to
    // the nearest whole byte
    size_t bpc = maxval <= UINT8_MAX ? 1 : 2;
    size_t rowsz = type == pnm_pbm ? div_ceil(f->width, 8)
                                   : f->width * bpc * (type == pnm_pgm ? 1 : 3);
    if (it->end < it->pos + f->height * rowsz)
        return PNM_EEOF;
    for (size_t y = 0; y < f->height; ++y) {
        argb_t* dst = f->data + y * f->width;
        const uint8_t* src = it->pos + y * rowsz;
        for (size_t x = 0; x < f->width; ++x) {
            argb_t pix = ARGB_SET_A(0xff);
            if (type == pnm_pbm) {
                const int bit = (src[x / 8] >> (7 - x % 8)) & 1;
                pix |= bit - 1;
            } else if (type == pnm_pgm) {
                int v = bpc == 1 ? src[x] : src[x] << 8 | src[x + 1];
                if (v > maxval)
                    return PNM_EOVF;
                if (maxval != UINT8_MAX)
                    v = div_near(v * UINT8_MAX, maxval);
                pix |= ARGB_SET_R(v) | ARGB_SET_G(v) | ARGB_SET_B(v);
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
                if (r > maxval || g > maxval || b > maxval)
                    return PNM_EOVF;
                if (maxval != UINT8_MAX) {
                    r = div_near(r * UINT8_MAX, maxval);
                    g = div_near(g * UINT8_MAX, maxval);
                    b = div_near(b * UINT8_MAX, maxval);
                }
                pix |= ARGB_SET_R(r) | ARGB_SET_G(g) | ARGB_SET_B(b);
            }
            dst[x] = pix;
        }
    }
    return 0;
}

enum loader_status decode_pnm(struct image* ctx, const uint8_t* data,
                              size_t size)
{
    struct pnm_iter it;
    bool plain;
    enum pnm_type type;
    int width, height, maxval, ret;

    if (size < 2 || data[0] != 'P') {
        return ldr_unsupported;
    }
    switch (data[1]) {
        case '1':
            plain = true;
            type = pnm_pbm;
            break;
        case '2':
            plain = true;
            type = pnm_pgm;
            break;
        case '3':
            plain = true;
            type = pnm_ppm;
            break;
        case '4':
            plain = false;
            type = pnm_pbm;
            break;
        case '5':
            plain = false;
            type = pnm_pgm;
            break;
        case '6':
            plain = false;
            type = pnm_ppm;
            break;
        default:
            return ldr_unsupported;
    }
    it.pos = data + 2;
    it.end = data + size;

    width = pnm_readint(&it, 0);
    if (width < 0) {
        image_print_error(ctx, "%s", pnm_strerror(width));
        return ldr_fmterror;
    }
    height = pnm_readint(&it, 0);
    if (height < 0) {
        image_print_error(ctx, "%s", pnm_strerror(height));
        return ldr_fmterror;
    }
    if (type == pnm_pbm) {
        maxval = 1;
    } else {
        maxval = pnm_readint(&it, 0);
        if (maxval < 0) {
            image_print_error(ctx, "%s", pnm_strerror(maxval));
            return ldr_fmterror;
        }
        if (!maxval || maxval > UINT16_MAX) {
            image_print_error(ctx, "invalid maxval");
            return ldr_fmterror;
        }
    }
    if (!plain) {
        // Again, the specifications technically allow for comments here, but no
        // other parsers support that (they treat that comment as image data),
        // so we won't allow one either
        const char c = *it.pos;
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            image_print_error(ctx, "single whitespace character expected");
            return ldr_fmterror;
        }
        ++it.pos;
    }
    if (!image_create_frame(ctx, width, height)) {
        return ldr_fmterror;
    }

    ret = plain ? decode_plain(ctx->frames, &it, type, maxval)
                : decode_raw(ctx->frames, &it, type, maxval);
    if (ret < 0) {
        image_print_error(ctx, "%s", pnm_strerror(ret));
        image_free_frames(ctx);
        return ldr_fmterror;
    }

    image_set_format(ctx, "P%cM (%s)",
                     type == pnm_pbm ? 'B' : (type == pnm_pgm ? 'G' : 'P'),
                     plain ? "ASCII" : "raw");

    return ldr_success;
}
