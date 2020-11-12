// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

//
// BMP image format support
//

#include "image_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define BITS_IN_BYTE 8

// Format name
static const char* const format_name = "BMP";

// BMP signature
static const uint8_t signature[] = { 'B', 'M' };

// BITMAPFILEHEADER
struct __attribute__((__packed__)) bmp_file_header {
    uint16_t type;
    uint32_t file_size;
    uint32_t reserved;
    uint32_t offset;
};

// BITMAPCOREINFO
struct __attribute__((__packed__)) bmp_core_info {
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

// implementation of struct loader::load
static cairo_surface_t* load(const char* file, const uint8_t* header, size_t header_len)
{
    cairo_surface_t* img = NULL;
    uint8_t* buffer = NULL;
    uint32_t* color_map = NULL;

    // check signature
    if (header_len < sizeof(signature) || memcmp(header, signature, sizeof(signature))) {
        return NULL;
    }

    const int fd = open(file, O_RDONLY);
    if (fd == -1) {
        load_error(format_name, errno, "Unable to open file");
        return NULL;
    }

    // read file/bmp headers
    struct bmp_file_header fhdr;
    if (read(fd, &fhdr, sizeof(fhdr)) != sizeof(fhdr)) {
        load_error(format_name, errno ? errno : ENODATA, "Unable to read file header");
        goto done;
    }
    struct bmp_core_info bmp;
    if (read(fd, &bmp, sizeof(bmp)) != sizeof(bmp)) {
        load_error(format_name, errno ? errno : ENODATA, "Unable to read bmp info");
        goto done;
    }

    // RLE is not supported yet
    if (bmp.compression != 0 /* BI_RGB */ && bmp.compression != 3 /* BI_BITFIELDS */) {
        load_error(format_name, 0, "Compression (%i) is not supported", bmp.compression);
        goto done;
    }

    // read color palette
    if (bmp.clr_palette) {
        if (lseek(fd, sizeof(struct bmp_file_header) + bmp.dib_size, SEEK_SET) == -1) {
            load_error(format_name, errno, "Unable to set file offset");
            goto done;
        }
        const size_t map_sz = bmp.clr_palette * sizeof(uint32_t);
        color_map = malloc(map_sz);
        if (!color_map) {
            load_error(format_name, errno, "Memory allocation error");
            goto done;
        }
        if (read(fd, color_map, map_sz) != (ssize_t)map_sz) {
            load_error(format_name, errno ? errno : ENODATA, "Unable to read palette");
            goto done;
        }
    }

    // read pixel data
    if (lseek(fd, fhdr.offset, SEEK_SET) == -1) {
        load_error(format_name, errno, "Unable to set file offset");
        goto done;
    }
    const size_t stride = 4 * ((bmp.width * bmp.bpp + 31) / 32);
    const size_t size = abs(bmp.height) * stride;
    buffer = malloc(size);
    if (!buffer) {
        load_error(format_name, errno, "Memory allocation error");
        goto done;
    }
    if (read(fd, buffer, size) != (ssize_t)size) {
        load_error(format_name, errno ? errno : ENODATA, "Unable to read pixel data");
        goto done;
    }

    // create canvas
    img = cairo_image_surface_create(
            bmp.bpp == 32 ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24,
            bmp.width, abs(bmp.height));
    if (cairo_surface_status(img) != CAIRO_STATUS_SUCCESS) {
        load_error(format_name, 0, "Unable to create surface: %s",
                   cairo_status_to_string(cairo_surface_status(img)));
        cairo_surface_destroy(img);
        img = NULL;
        goto done;
    }

    // default mask 5:5:5
    if (!bmp.red_mask && !bmp.green_mask && !bmp.blue_mask) {
        bmp.red_mask = 0x001f;
        bmp.green_mask = 0x03e0;
        bmp.blue_mask = 0x7c00;
    }

    // color channels
    const ssize_t red_shift = mask_shift(bmp.red_mask);
    const ssize_t green_shift = mask_shift(bmp.green_mask);
    const ssize_t blue_shift = mask_shift(bmp.blue_mask);

    // flip and convert to argb (cairo internal format)
    uint8_t* dst_data = cairo_image_surface_get_data(img);
    const size_t dst_stride = cairo_image_surface_get_stride(img);
    for (size_t y = 0; y < abs(bmp.height); ++y) {
        uint8_t* dst_y = dst_data + y * dst_stride;
        uint8_t* src_y;
        if (bmp.height > 0) {
            src_y = buffer + (bmp.height - y - 1) * stride;
        } else {
            src_y = buffer + y * stride; // top-down format (rarely used)
        }
        for (size_t x = 0; x < bmp.width; ++x) {
            uint8_t a = 0xff, r = 0, g = 0, b = 0;
            const uint8_t* src = src_y + x * (bmp.bpp / BITS_IN_BYTE);
            switch (bmp.bpp) {
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
                    r = red_shift > 0 ? (val & bmp.red_mask) >> red_shift :
                                        (val & bmp.red_mask) << -red_shift;
                    g = green_shift > 0 ? (val & bmp.green_mask) >> green_shift :
                                          (val & bmp.green_mask) << -green_shift;
                    b = blue_shift > 0 ? (val & bmp.blue_mask) >> blue_shift :
                                         (val & bmp.blue_mask) << -blue_shift;
                    }
                    break;
                default: {
                    // indexed colors
                    const size_t bits_offset = x * bmp.bpp;
                    const size_t byte_offset = bits_offset / BITS_IN_BYTE;
                    const size_t start_bit = bits_offset - byte_offset * BITS_IN_BYTE;
                    const uint8_t val =
                        (*(src_y + byte_offset) >> (BITS_IN_BYTE - bmp.bpp - start_bit)) &
                        (0xff >> (BITS_IN_BYTE - bmp.bpp));
                    if (color_map && val < bmp.clr_palette) {
                        const uint8_t* clr = (uint8_t*)&color_map[val];
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

    cairo_surface_mark_dirty(img);

done:
    if (color_map) {
        free(color_map);
    }
    if (buffer) {
        free(buffer);
    }
    close(fd);

    return img;
}

// declare format
const struct loader bmp_loader = {
    .format = format_name,
    .load = load
};
