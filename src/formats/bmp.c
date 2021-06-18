// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

//
// BMP image format support
//

#include "../image.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define BITS_IN_BYTE 8

// BMP signature
static const uint8_t signature[] = { 'B', 'M' };

// BITMAPFILEHEADER
struct __attribute__((__packed__)) bmp_header {
    uint16_t type;
    uint32_t file_size;
    uint32_t reserved;
    uint32_t offset;
};

// BITMAPCOREINFO
struct __attribute__((__packed__)) bmp_info {
    uint32_t dib_size;
    uint32_t width;
    int32_t  height;
    uint16_t planes;
    uint16_t bpp;
    uint32_t compression;
    uint32_t img_size;
    uint32_t hres;
    uint32_t vres;
    uint32_t clr_palette;
    uint32_t clr_important;
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
};

/**
 * Get number of the consecutive zero bits (trailing) on the right.
 * @param[in] val source value
 * @return number of zero bits
 */
static size_t right_zeros(uint32_t val)
{
    size_t count = sizeof(uint32_t) * BITS_IN_BYTE;
    val &= -(int32_t)val;
    if (val) --count;
    if (val & 0x0000ffff) count -= 16;
    if (val & 0x00ff00ff) count -= 8;
    if (val & 0x0f0f0f0f) count -= 4;
    if (val & 0x33333333) count -= 2;
    if (val & 0x55555555) count -= 1;
    return count;
}

/**
 * Get number of bits set.
 * @param[in] val source value
 * @return number of bits set
 */
static size_t bits_set(uint32_t val)
{
    val = val - ((val >> 1) & 0x55555555);
    val = (val & 0x33333333) + ((val >> 2) & 0x33333333);
    return (((val + (val >> 4)) & 0xf0f0f0f) * 0x1010101) >> 24;
}

/**
 * Get shift size for color channel.
 * @param[in] val color channel mask
 * @return shift size: positive=right, negative=left
 */
static ssize_t mask_shift(uint32_t mask)
{
    const ssize_t start = right_zeros(mask) + bits_set(mask);
    return start - BITS_IN_BYTE;
}

// BMP loader implementation
struct image* load_bmp(const uint8_t* data, size_t size)
{
    // check signature
    if (size < sizeof(struct bmp_header) + sizeof(struct bmp_info) ||
        memcmp(data, signature, sizeof(signature))) {
        return NULL;
    }

    const struct bmp_header* header = (const struct bmp_header*)data;
    const struct bmp_info* bmp = (const struct bmp_info*)(data + sizeof(struct bmp_header));
    const size_t stride = 4 * ((bmp->width * bmp->bpp + 31) / 32);

    // RLE is not supported yet
    if (bmp->compression != 0 /* BI_RGB */ && bmp->compression != 3 /* BI_BITFIELDS */) {
        fprintf(stderr, "BMP compression (%i) not supported\n", bmp->compression);
        return NULL;
    }

    // check size of input buffer
    if (size < header->offset + abs(bmp->height) * stride) {
        fprintf(stderr, "Invalid BMP format\n");
        return NULL;
    }

    // create image instance
    const cairo_format_t fmt = bmp->bpp == 32 ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;
    struct image* img = create_image(fmt, bmp->width, abs(bmp->height));
    if (!img) {
        return NULL;
    }
    set_image_meta(img, "BMP %dbit", bmp->bpp);

    // color channels (default 5:5:5)
    const bool def_mask = !bmp->red_mask && !bmp->green_mask && !bmp->blue_mask;
    const uint32_t mask_r = def_mask ? 0x001f : bmp->red_mask;
    const uint32_t mask_g = def_mask ? 0x03e0 : bmp->green_mask;
    const uint32_t mask_b = def_mask ? 0x7c00 : bmp->blue_mask;
    const ssize_t shift_r = mask_shift(mask_r);
    const ssize_t shift_g = mask_shift(mask_g);
    const ssize_t shift_b = mask_shift(mask_b);

    // flip and convert to argb (cairo internal format)
    uint8_t* dst_data = cairo_image_surface_get_data(img->surface);
    const size_t dst_stride = cairo_image_surface_get_stride(img->surface);
    for (size_t y = 0; y < abs(bmp->height); ++y) {
        uint8_t* dst_y = dst_data + y * dst_stride;
        const uint8_t* src_y = data + header->offset;
        if (bmp->height > 0) {
            src_y += (bmp->height - y - 1) * stride;
        } else {
            src_y += y * stride; // top-down format (rarely used)
        }
        for (size_t x = 0; x < bmp->width; ++x) {
            uint8_t a = 0xff, r = 0, g = 0, b = 0;
            const uint8_t* src = src_y + x * (bmp->bpp / BITS_IN_BYTE);
            switch (bmp->bpp) {
                case 32:
                    a = src[3];
                    r = src[2];
                    g = src[1];
                    b = src[0];
                    break;
                case 24:
                    r = src[2];
                    g = src[1];
                    b = src[0];
                    break;
                case 16: {
                    const uint16_t val = *(uint16_t*)src;
                    r = shift_r > 0 ? (val & mask_r) >> shift_r :
                                      (val & mask_r) << -shift_r;
                    g = shift_g > 0 ? (val & mask_g) >> shift_g :
                                      (val & mask_g) << -shift_g;
                    b = shift_b > 0 ? (val & mask_b) >> shift_b :
                                      (val & mask_b) << -shift_b;
                    }
                    break;
                default: {
                    // indexed colors
                    const size_t bits_offset = x * bmp->bpp;
                    const size_t byte_offset = bits_offset / BITS_IN_BYTE;
                    const size_t start_bit = bits_offset - byte_offset * BITS_IN_BYTE;
                    const uint8_t val =
                        (*(src_y + byte_offset) >> (BITS_IN_BYTE - bmp->bpp - start_bit)) &
                        (0xff >> (BITS_IN_BYTE - bmp->bpp));
                    if (val < bmp->clr_palette) {
                        const uint8_t* clr = data + sizeof(struct bmp_header) +
                                             bmp->dib_size + val * sizeof(uint32_t);
                        r = clr[2];
                        g = clr[1];
                        b = clr[0];
                    } else {
                        // color without palette?
                        r = ((val >> 0) & 1) * 0xff;
                        g = ((val >> 1) & 1) * 0xff;
                        b = ((val >> 2) & 1) * 0xff;
                    }
                }
            }
            uint32_t* dst_x = (uint32_t*)(dst_y + x * 4 /*argb*/);
            *dst_x = a << 24 | r << 16 | g << 8 | b;
        }
    }

    cairo_surface_mark_dirty(img->surface);

    return img;
}
