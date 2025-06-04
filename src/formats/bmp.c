// SPDX-License-Identifier: MIT
// BMP format decoder.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include <stdlib.h>

// BMP type id
#define BMP_TYPE ('B' | ('M' << 8))

// Compression types
#define BI_RGB       0
#define BI_RLE8      1
#define BI_RLE4      2
#define BI_BITFIELDS 3

// RLE escape codes
#define RLE_ESC_EOL   0
#define RLE_ESC_EOF   1
#define RLE_ESC_DELTA 2

// Default mask for 16-bit images
#define MASK555_RED   0x7c00
#define MASK555_GREEN 0x03e0
#define MASK555_BLUE  0x001f
#define MASK555_ALPHA 0x0000

// Sizes of DIB Headers
#define BITMAPINFOHEADER_SIZE   0x28
#define BITMAPINFOV2HEADER_SIZE 0x34
#define BITMAPINFOV3HEADER_SIZE 0x38
#define BITMAPINFOV4HEADER_SIZE 0x6C
#define BITMAPINFOV5HEADER_SIZE 0x7C

#define BITS_PER_BYTE 8

// Bitmap file header: BITMAPFILEHEADER
struct __attribute__((__packed__)) bmp_file {
    uint16_t type;
    uint32_t file_size;
    uint32_t reserved;
    uint32_t offset;
};

// Bitmap info: BITMAPINFOHEADER
struct __attribute__((__packed__)) bmp_info {
    uint32_t dib_size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bpp;
    uint32_t compression;
    uint32_t img_size;
    uint32_t hres;
    uint32_t vres;
    uint32_t clr_palette;
    uint32_t clr_important;
};

// Masks used for for 16bit images
struct __attribute__((__packed__)) bmp_mask {
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    uint32_t alpha;
};

// Color palette
struct bmp_palette {
    const uint32_t* table;
    size_t size;
};

/**
 * Get number of the consecutive zero bits (trailing) on the right.
 * @param val source value
 * @return number of zero bits
 */
static inline size_t right_zeros(uint32_t val)
{
    size_t count = sizeof(uint32_t) * BITS_PER_BYTE;
    val &= -(int32_t)val;
    if (val) {
        --count;
    }
    if (val & 0x0000ffff) {
        count -= 16;
    }
    if (val & 0x00ff00ff) {
        count -= 8;
    }
    if (val & 0x0f0f0f0f) {
        count -= 4;
    }
    if (val & 0x33333333) {
        count -= 2;
    }
    if (val & 0x55555555) {
        count -= 1;
    }
    return count;
}

/**
 * Get number of bits set.
 * @param val source value
 * @return number of bits set
 */
static inline size_t bits_set(uint32_t val)
{
    val = val - ((val >> 1) & 0x55555555);
    val = (val & 0x33333333) + ((val >> 2) & 0x33333333);
    return (((val + (val >> 4)) & 0xf0f0f0f) * 0x1010101) >> 24;
}

/**
 * Get shift size for color channel.
 * @param val color channel mask
 * @return shift size: positive=right, negative=left
 */
static inline ssize_t mask_shift(uint32_t mask)
{
    const ssize_t start = right_zeros(mask) + bits_set(mask);
    return start - BITS_PER_BYTE;
}

/**
 * Decode bitmap with masked colors.
 * @param pm target pixmap
 * @param bmp bitmap info
 * @param mask channels mask
 * @param buffer input bitmap buffer
 * @param buffer_sz size of buffer
 * @return false if input buffer has errors
 */
static bool decode_masked(struct pixmap* pm, const struct bmp_info* bmp,
                          const struct bmp_mask* mask, const uint8_t* buffer,
                          size_t buffer_sz)
{
    const bool default_mask = !mask ||
        (mask->red == 0 && mask->green == 0 && mask->blue == 0 &&
         mask->alpha == 0);

    const uint32_t mask_r = default_mask ? MASK555_RED : mask->red;
    const uint32_t mask_g = default_mask ? MASK555_GREEN : mask->green;
    const uint32_t mask_b = default_mask ? MASK555_BLUE : mask->blue;
    const uint32_t mask_a = default_mask ? MASK555_ALPHA : mask->alpha;
    const ssize_t shift_r = mask_shift(mask_r);
    const ssize_t shift_g = mask_shift(mask_g);
    const ssize_t shift_b = mask_shift(mask_b);
    const ssize_t shift_a = mask_shift(mask_a);

    const size_t stride = 4 * ((bmp->width * bmp->bpp + 31) / 32);

    // check size of source buffer
    if (buffer_sz < pm->height * stride) {
        return false;
    }

    for (size_t y = 0; y < pm->height; ++y) {
        argb_t* dst = &pm->data[y * pm->width];
        const uint8_t* src_y = buffer + y * stride;
        for (size_t x = 0; x < pm->width; ++x) {
            const uint8_t* src = src_y + x * (bmp->bpp / BITS_PER_BYTE);
            uint32_t m, r, g, b, a;
            if (bmp->bpp == 32) {
                m = *(uint32_t*)src;
            } else if (bmp->bpp == 16) {
                m = *(uint16_t*)src;
            } else {
                return false;
            }
            r = m & mask_r;
            g = m & mask_g;
            b = m & mask_b;
            r = 0xff & (shift_r > 0 ? r >> shift_r : r << -shift_r);
            g = 0xff & (shift_g > 0 ? g >> shift_g : g << -shift_g);
            b = 0xff & (shift_b > 0 ? b >> shift_b : b << -shift_b);

            if (mask_a) {
                a = m & mask_a;
                a = 0xff & (shift_a > 0 ? a >> shift_a : a << -shift_a);
            } else {
                a = 0xff;
            }

            dst[x] =
                ARGB_SET_A(a) | ARGB_SET_R(r) | ARGB_SET_G(g) | ARGB_SET_B(b);
        }
    }

    return true;
}

/**
 * Decode RLE compressed bitmap.
 * @param pm target pixmap
 * @param bmp bitmap info
 * @param palette color palette
 * @param buffer input bitmap buffer
 * @param buffer_sz size of buffer
 * @return false if input buffer has errors
 */
static bool decode_rle(struct pixmap* pm, const struct bmp_info* bmp,
                       const struct bmp_palette* palette, const uint8_t* buffer,
                       size_t buffer_sz)
{
    size_t x = 0, y = 0;
    size_t buffer_pos = 0;

    while (buffer_pos + 2 <= buffer_sz) {
        uint8_t rle1 = buffer[buffer_pos++];
        const uint8_t rle2 = buffer[buffer_pos++];
        if (rle1 == 0) {
            // escape code
            if (rle2 == RLE_ESC_EOL) {
                x = 0;
                ++y;
            } else if (rle2 == RLE_ESC_EOF) {
                // remove alpha channel
                argb_t* ptr = pm->data;
                while (ptr < pm->data + pm->width * pm->height) {
                    *ptr |= ARGB_SET_A(0xff);
                    ++ptr;
                }
                return true;
            } else if (rle2 == RLE_ESC_DELTA) {
                if (buffer_pos + 2 >= buffer_sz) {
                    return false;
                }
                x += buffer[buffer_pos++];
                y += buffer[buffer_pos++];
            } else {
                // absolute mode
                if (buffer_pos +
                        (bmp->compression == BI_RLE4 ? rle2 / 2 : rle2) >
                    buffer_sz) {
                    return false;
                }
                if (x + rle2 > pm->width || y >= pm->height) {
                    return false;
                }
                uint8_t val = 0;
                for (size_t i = 0; i < rle2; ++i) {
                    uint8_t index;
                    if (bmp->compression == BI_RLE8) {
                        index = buffer[buffer_pos++];
                    } else {
                        if (i & 1) {
                            index = val & 0x0f;
                        } else {
                            val = buffer[buffer_pos++];
                            index = val >> 4;
                        }
                    }
                    if (index >= palette->size) {
                        return false;
                    }
                    pm->data[y * bmp->width + x] = palette->table[index];
                    ++x;
                }
                if ((bmp->compression == BI_RLE8 && rle2 & 1) ||
                    (bmp->compression == BI_RLE4 &&
                     ((rle2 & 3) == 1 || (rle2 & 3) == 2))) {
                    ++buffer_pos; // zero-padded 16-bit
                }
            }
        } else {
            // encoded mode
            if (x + rle1 > pm->width) {
                rle1 = pm->width - x;
            }
            if (y >= pm->height) {
                return false;
            }
            if (bmp->compression == BI_RLE8) {
                // 8 bpp
                if (rle2 >= palette->size) {
                    return false;
                }
                for (size_t i = 0; i < rle1; ++i) {
                    pm->data[y * pm->width + x] = palette->table[rle2];
                    ++x;
                }
            } else {
                // 4 bpp
                const uint8_t index[] = { rle2 >> 4, rle2 & 0x0f };
                if (index[0] >= palette->size || index[1] >= palette->size) {
                    return false;
                }
                for (size_t i = 0; i < rle1; ++i) {
                    pm->data[y * pm->width + x] = palette->table[index[i & 1]];
                    ++x;
                }
            }
        }
    }

    return false;
}

/**
 * Decode uncompressed bitmap.
 * @param pm target pixmap
 * @param palette color palette
 * @param buffer input bitmap buffer
 * @param buffer_sz size of buffer
 * @param decoded output data buffer
 * @return false if input buffer has errors
 */
static bool decode_rgb(struct pixmap* pm, const struct bmp_info* bmp,
                       const struct bmp_palette* palette, const uint8_t* buffer,
                       size_t buffer_sz)
{
    const size_t stride = 4 * ((bmp->width * bmp->bpp + 31) / 32);

    // check size of source buffer
    if (buffer_sz < pm->height * stride) {
        return false;
    }

    for (size_t y = 0; y < pm->height; ++y) {
        argb_t* dst = &pm->data[y * pm->width];
        const uint8_t* src_y = buffer + y * stride;
        for (size_t x = 0; x < pm->width; ++x) {
            const uint8_t* src = src_y + x * (bmp->bpp / BITS_PER_BYTE);
            if (bmp->bpp == 32) {
                dst[x] = ARGB_SET_A(0xff) | *(uint32_t*)src;
            } else if (bmp->bpp == 24) {
                dst[x] = ARGB_SET_A(0xff) | *(uint32_t*)src;
            } else if (bmp->bpp == 8 || bmp->bpp == 4 || bmp->bpp == 1) {
                // indexed colors
                const size_t bits_offset = x * bmp->bpp;
                const size_t byte_offset = bits_offset / BITS_PER_BYTE;
                const size_t start_bit =
                    bits_offset - byte_offset * BITS_PER_BYTE;
                const uint8_t index = (*(src_y + byte_offset) >>
                                       (BITS_PER_BYTE - bmp->bpp - start_bit)) &
                    (0xff >> (BITS_PER_BYTE - bmp->bpp));

                if (index >= palette->size) {
                    return false;
                }
                dst[x] = ARGB_SET_A(0xff) | palette->table[index];
            } else {
                return false;
            }
        }
    }

    return true;
}

// BMP loader implementation
enum image_status decode_bmp(struct imgdata* img, const uint8_t* data,
                             size_t size)
{
    const char* format;
    struct pixmap* pm;
    const struct bmp_file* hdr;
    const struct bmp_info* bmp;
    const void* color_data;
    size_t color_data_sz;
    struct bmp_palette palette;
    const uint32_t* mask_location;
    struct bmp_mask mask;
    bool rc;

    hdr = (const struct bmp_file*)data;
    bmp = (const struct bmp_info*)(data + sizeof(*hdr));

    // check format
    if (size < sizeof(*hdr) || hdr->type != BMP_TYPE) {
        return imgload_unsupported;
    }
    if (hdr->offset >= size ||
        hdr->offset < sizeof(struct bmp_file) + sizeof(struct bmp_info)) {
        return imgload_fmterror;
    }
    if (bmp->dib_size > hdr->offset) {
        return imgload_fmterror;
    }

    pm = image_alloc_frame(img, bmp->bpp == 32 ? pixmap_argb : pixmap_xrgb,
                           abs(bmp->width), abs(bmp->height));
    if (!pm) {
        return imgload_fmterror;
    }

    color_data = (const uint8_t*)bmp + bmp->dib_size;
    color_data_sz = hdr->offset - sizeof(struct bmp_file) - bmp->dib_size;
    palette.table = color_data;
    palette.size = color_data_sz / sizeof(uint32_t);

    // create mask
    if (bmp->dib_size > BITMAPINFOHEADER_SIZE) {
        mask_location = (const uint32_t*)(bmp + 1);
    } else {
        mask_location =
            (color_data_sz >= 3 * sizeof(uint32_t) ? color_data : NULL);
    }

    if (!mask_location) {
        mask.red = mask.green = mask.blue = mask.alpha = 0;
    } else {
        mask.red = mask_location[0];
        mask.green = mask_location[1];
        mask.blue = mask_location[2];
        mask.alpha =
            bmp->dib_size > BITMAPINFOV2HEADER_SIZE ? mask_location[3] : 0;
    }

    // decode bitmap
    if (bmp->compression == BI_BITFIELDS || bmp->bpp == 16) {
        rc = decode_masked(pm, bmp, &mask, data + hdr->offset,
                           size - hdr->offset);
        format = "masked";
    } else if (bmp->compression == BI_RLE8 || bmp->compression == BI_RLE4) {
        rc = decode_rle(pm, bmp, &palette, data + hdr->offset,
                        size - hdr->offset);
        format = "RLE";
    } else if (bmp->compression == BI_RGB) {
        rc = decode_rgb(pm, bmp, &palette, data + hdr->offset,
                        size - hdr->offset);
        format = "uncompressed";
    } else {
        rc = false;
    }

    if (rc) {
        image_set_format(img, "BMP %dbit %s", bmp->bpp, format);
        if (bmp->height > 0) {
            pixmap_flip_vertical(pm);
        }
    }

    return (rc ? imgload_success : imgload_fmterror);
}
