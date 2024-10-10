// SPDX-License-Identifier: MIT
// QOI format decoder.
// https://qoiformat.org/

#include "../loader.h"

#include <string.h>

#define QOI_OP_INDEX 0x00 /* 00xxxxxx */
#define QOI_OP_DIFF  0x40 /* 01xxxxxx */
#define QOI_OP_LUMA  0x80 /* 10xxxxxx */
#define QOI_OP_RUN   0xc0 /* 11xxxxxx */
#define QOI_OP_RGB   0xfe /* 11111110 */
#define QOI_OP_RGBA  0xff /* 11111111 */

#define QOI_MASK_2 0xc0 /* 11000000 */

#define QOI_MAP_LEN 64
#define QOI_COLOR_HASH(C) \
    (C.rgba.r * 3 + C.rgba.g * 5 + C.rgba.b * 7 + C.rgba.a * 11)
#define QOI_MAGIC                                                           \
    (((uint32_t)'q') << 24 | ((uint32_t)'o') << 16 | ((uint32_t)'i') << 8 | \
     ((uint32_t)'f'))
#define QOI_HEADER_SIZE  14
#define QOI_PADDING_SIZE 8

/* 2GB is the max file size that this implementation can safely handle. We guard
against anything larger than that, assuming the worst case with 5 bytes per
pixel, rounded down to a nice clean value. 400 million pixels ought to be
enough for anybody. */
#define QOI_PIXELS_MAX ((uint32_t)400000000)

union qoi_rgba_t {
    struct {
        uint8_t b, g, r, a;
    } rgba;
    argb_t v;
};

static uint32_t qoi_read_32(const uint8_t* bytes, size_t* bytes_pos)
{
    uint32_t a = bytes[(*bytes_pos)++];
    uint32_t b = bytes[(*bytes_pos)++];
    uint32_t c = bytes[(*bytes_pos)++];
    uint32_t d = bytes[(*bytes_pos)++];
    return a << 24 | b << 16 | c << 8 | d;
}

// QOI loader implementation
enum loader_status decode_qoi(struct image* ctx, const uint8_t* bytes,
                              size_t size)
{
    uint32_t header_magic;
    uint32_t width;
    uint32_t height;
    uint8_t channels;
    uint8_t colorspace;
    argb_t* pixels;
    union qoi_rgba_t pixel_map[QOI_MAP_LEN];
    union qoi_rgba_t cur_pixel;
    size_t px_len, chunks_len, px_pos;
    size_t bytes_pos = 0, run_len = 0;

    if (size < QOI_HEADER_SIZE + QOI_PADDING_SIZE) {
        return ldr_unsupported;
    }

    header_magic = qoi_read_32(bytes, &bytes_pos);
    width = qoi_read_32(bytes, &bytes_pos);
    height = qoi_read_32(bytes, &bytes_pos);
    channels = bytes[bytes_pos++];
    colorspace = bytes[bytes_pos++];

    if (width == 0 || height == 0 || channels < 3 || channels > 4 ||
        colorspace > 1 || header_magic != QOI_MAGIC ||
        height >= QOI_PIXELS_MAX / width) {
        return ldr_unsupported;
    }

    image_set_format(ctx, "QOI %dchannels", channels);
    ctx->alpha = channels == 4;

    if (!image_allocate_frame(ctx, width, height)) {
        return ldr_fmterror;
    }

    pixels = ctx->frames[0].pm.data;

    memset(pixel_map, 0, sizeof(pixel_map));
    cur_pixel.rgba.r = 0;
    cur_pixel.rgba.g = 0;
    cur_pixel.rgba.b = 0;
    cur_pixel.rgba.a = 255;

    px_len = width * height;
    chunks_len = size - QOI_PADDING_SIZE;
    for (px_pos = 0; px_pos < px_len; px_pos += 1) {
        if (run_len > 0) {
            run_len--;
        } else if (bytes_pos < chunks_len) {
            uint8_t b1 = bytes[bytes_pos++];
            if (b1 == QOI_OP_RGB) {
                cur_pixel.rgba.r = bytes[bytes_pos++];
                cur_pixel.rgba.g = bytes[bytes_pos++];
                cur_pixel.rgba.b = bytes[bytes_pos++];
            } else if (b1 == QOI_OP_RGBA) {
                cur_pixel.rgba.r = bytes[bytes_pos++];
                cur_pixel.rgba.g = bytes[bytes_pos++];
                cur_pixel.rgba.b = bytes[bytes_pos++];
                cur_pixel.rgba.a = bytes[bytes_pos++];
            } else if ((b1 & QOI_MASK_2) == QOI_OP_INDEX) {
                cur_pixel = pixel_map[b1];
            } else if ((b1 & QOI_MASK_2) == QOI_OP_DIFF) {
                cur_pixel.rgba.r += ((b1 >> 4) & 0x03) - 2;
                cur_pixel.rgba.g += ((b1 >> 2) & 0x03) - 2;
                cur_pixel.rgba.b += (b1 & 0x03) - 2;
            } else if ((b1 & QOI_MASK_2) == QOI_OP_LUMA) {
                uint8_t b2 = bytes[bytes_pos++];
                uint8_t vg = (b1 & 0x3f) - 32;
                cur_pixel.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
                cur_pixel.rgba.g += vg;
                cur_pixel.rgba.b += vg - 8 + (b2 & 0x0f);
            } else if ((b1 & QOI_MASK_2) == QOI_OP_RUN) {
                run_len = (b1 & 0x3f);
            }
            pixel_map[QOI_COLOR_HASH(cur_pixel) % QOI_MAP_LEN] = cur_pixel;
        }
        pixels[px_pos] = cur_pixel.v;
    }
    return ldr_success;
}
