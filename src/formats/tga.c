// SPDX-License-Identifier: MIT
// Truevision TGA format decoder.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "../loader.h"

#include <string.h>

/** TGA file header. */
struct __attribute__((__packed__)) tga_header {
    uint8_t id_len;
    uint8_t clrmap_type;
    uint8_t image_type;

    uint16_t cm_index;
    uint16_t cm_size;
    uint8_t cm_bpc;

    uint16_t origin_x;
    uint16_t origin_y;
    uint16_t width;
    uint16_t height;
    uint8_t bpp;
    uint8_t desc;
};

#define TGA_COLORMAP 1 // color map present flag

#define TGA_UNC_CM 1  // uncompressed color-mapped
#define TGA_UNC_TC 2  // uncompressed true-color
#define TGA_UNC_GS 3  // uncompressed grayscale
#define TGA_RLE_CM 9  // run-length encoded color-mapped
#define TGA_RLE_TC 10 // run-length encoded true-color
#define TGA_RLE_GS 11 // run-length encoded grayscale

#define TGA_ORDER_R2L (1 << 4) // right-to-left pixel ordering
#define TGA_ORDER_T2B (1 << 5) // top-to-bottom pixel ordering

#define TGA_PACKET_RLE (1 << 7) // rle/raw field
#define TGA_PACKET_LEN 0x7f     // length mask

/**
 * Get pixel color from data stream.
 * @param data pointer to stream data
 * @param bpp bits per pixel
 * @return color
 */
static inline argb_t get_pixel(const uint8_t* data, size_t bpp)
{
    argb_t pixel;

    switch (bpp) {
        case 8:
            pixel = ARGB_SET_A(0xff) | ARGB_SET_R(data[0]) |
                ARGB_SET_G(data[0]) | ARGB_SET_B(data[0]);
            break;
        case 15:
        case 16:
            pixel = ARGB_SET_A(0xff) | ARGB_SET_G(data[0] & 0xf8) |
                ARGB_SET_B((data[0] << 5) | ((data[1] & 0xc0) >> 2)) |
                ARGB_SET_R((data[1] & 0x3e) << 2);
            break;
        case 24:
            pixel = ARGB_SET_A(0xff) | ARGB_SET_R(data[2]) |
                ARGB_SET_G(data[1]) | ARGB_SET_B(data[0]);
            break;
        default:
            pixel = *(const argb_t*)data;
            break;
    }

    return pixel;
}

/**
 * Decode uncompressed image.
 * @param pm destination pixmap
 * @param tga source image descriptor
 * @param colormap color map
 * @param data pointer to image data
 * @param size size of image data in bytes
 * @return true if image decoded successfully
 */
static bool decode_unc(struct pixmap* pm, const struct tga_header* tga,
                       const uint8_t* colormap, const uint8_t* data,
                       size_t size)
{
    const uint8_t bytes_per_pixel = tga->bpp / 8 + (tga->bpp % 8 ? 1 : 0);
    const size_t num_pixels = pm->width * pm->height;
    const size_t data_size = num_pixels * bytes_per_pixel;

    if (data_size > size) {
        return false;
    }

    if (tga->bpp == 32) {
        memcpy(pm->data, data, data_size);
    } else {
        const uint8_t cm_bpp = tga->cm_bpc / 8 + (tga->cm_bpc % 8 ? 1 : 0);
        for (size_t i = 0; i < num_pixels; ++i) {
            const uint8_t* src = data + i * bytes_per_pixel;
            if (!colormap) {
                pm->data[i] = get_pixel(src, tga->bpp);
            } else {
                const uint8_t* entry = colormap + cm_bpp * (*src);
                if (entry + cm_bpp > data) {
                    return false;
                }
                pm->data[i] = get_pixel(entry, tga->cm_bpc);
            }
        }
    }

    return true;
}

/**
 * Decode RLE compressed image.
 * @param pm destination pixmap
 * @param tga source image descriptor
 * @param colormap color map
 * @param data pointer to image data
 * @param size size of image data in bytes
 * @return true if image decoded successfully
 */
static bool decode_rle(struct pixmap* pm, const struct tga_header* tga,
                       const uint8_t* colormap, const uint8_t* data,
                       size_t size)
{
    const uint8_t cm_bpp = tga->cm_bpc / 8 + (tga->cm_bpc % 8 ? 1 : 0);
    const uint8_t bytes_per_pixel = tga->bpp / 8 + (tga->bpp % 8 ? 1 : 0);
    const argb_t* pm_end = pm->data + pm->width * pm->height;
    argb_t* pixel = pm->data;
    size_t pos = 0;

    while (pixel < pm_end) {
        const uint8_t pack = data[pos++];
        const bool is_rle = (pack & TGA_PACKET_RLE);
        size_t len = (pack & TGA_PACKET_LEN) + 1;

        while (len--) {
            if (pos + bytes_per_pixel > size) {
                return false;
            }

            if (!colormap) {
                *pixel = get_pixel(data + pos, tga->bpp);
            } else {
                const uint8_t* entry = colormap + cm_bpp * data[pos];
                if (entry + cm_bpp > data) {
                    return false;
                }
                *pixel = get_pixel(entry, tga->cm_bpc);
            }

            if (pixel++ >= pm_end) {
                break;
            }
            if (!is_rle) {
                pos += bytes_per_pixel;
            }
        }
        if (is_rle) {
            pos += bytes_per_pixel;
        }
    }

    return true;
}

// TGA loader implementation
enum loader_status decode_tga(struct image* ctx, const uint8_t* data,
                              size_t size)
{
    const struct tga_header* tga = (const struct tga_header*)data;
    const uint8_t* colormap = NULL;
    size_t colormap_sz = 0;
    const char* type_name = NULL;
    bool rc = false;
    size_t data_offset;
    struct pixmap* pm;

    // check type
    if (size < sizeof(struct tga_header) ||
        (tga->image_type != TGA_UNC_CM && tga->image_type != TGA_UNC_TC &&
         tga->image_type != TGA_UNC_GS && tga->image_type != TGA_RLE_CM &&
         tga->image_type != TGA_RLE_TC && tga->image_type != TGA_RLE_GS)) {
        return ldr_unsupported;
    }
    // check image params
    if (tga->width == 0 || tga->height == 0 ||
        (tga->bpp != 8 && tga->bpp != 15 && tga->bpp != 16 && tga->bpp != 24 &&
         tga->bpp != 32)) {
        return ldr_unsupported;
    }

    // get color map
    switch (tga->image_type) {
        case TGA_UNC_CM:
        case TGA_RLE_CM:
            if (!(tga->clrmap_type & TGA_COLORMAP) || !tga->cm_size ||
                !tga->cm_bpc) {
                return ldr_unsupported;
            }
            colormap_sz =
                tga->cm_size * (tga->cm_bpc / 8 + (tga->cm_bpc % 8 ? 1 : 0));
            colormap = data + sizeof(struct tga_header) + tga->id_len;
            break;
        default:
            if (tga->clrmap_type & TGA_COLORMAP || tga->cm_size ||
                tga->cm_bpc) {
                return ldr_unsupported;
            }
            break;
    }

    // get pixel array offset
    data_offset = sizeof(struct tga_header) + tga->id_len + colormap_sz;
    if (data_offset >= size) {
        return ldr_unsupported;
    }
    data += data_offset;
    size -= data_offset;

    // decode image
    pm = image_allocate_frame(ctx, tga->width, tga->height);
    if (!pm) {
        return ldr_fmterror;
    }
    switch (tga->image_type) {
        case TGA_UNC_CM:
        case TGA_UNC_TC:
        case TGA_UNC_GS:
            rc = decode_unc(pm, tga, colormap, data, size);
            break;
        case TGA_RLE_CM:
        case TGA_RLE_TC:
        case TGA_RLE_GS:
            rc = decode_rle(pm, tga, colormap, data, size);
            break;
    }
    if (!rc) {
        image_free_frames(ctx);
        return ldr_fmterror;
    }

    // fix orientation
    if (!(tga->desc & TGA_ORDER_T2B)) {
        pixmap_flip_vertical(pm);
    }
    if (tga->desc & TGA_ORDER_R2L) {
        pixmap_flip_horizontal(pm);
    }

    // set image meta data
    switch (tga->image_type) {
        case TGA_UNC_CM:
            type_name = "uncompressed color-mapped";
            break;
        case TGA_UNC_TC:
            type_name = "uncompressed true-color";
            break;
        case TGA_UNC_GS:
            type_name = "uncompressed grayscale";
            break;
        case TGA_RLE_CM:
            type_name = "RLE color-mapped";
            break;
        case TGA_RLE_TC:
            type_name = "RLE true-color";
            break;
        case TGA_RLE_GS:
            type_name = "RLE grayscale";
            break;
    }
    image_set_format(ctx, "TARGA %dbpp, %s", tga->bpp, type_name);
    ctx->alpha = (tga->bpp == 32);

    return ldr_success;
}
