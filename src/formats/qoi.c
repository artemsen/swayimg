// SPDX-License-Identifier: CC0
// QOI format decoder.
// https://qoiformat.org/

#include <string.h>
#include "../loader.h"

#define QOI_OP_INDEX  0x00 /* 00xxxxxx */
#define QOI_OP_DIFF   0x40 /* 01xxxxxx */
#define QOI_OP_LUMA   0x80 /* 10xxxxxx */
#define QOI_OP_RUN    0xc0 /* 11xxxxxx */
#define QOI_OP_RGB    0xfe /* 11111110 */
#define QOI_OP_RGBA   0xff /* 11111111 */

#define QOI_MASK_2    0xc0 /* 11000000 */

#define QOI_COLOR_HASH(C) (C.rgba.r*3 + C.rgba.g*5 + C.rgba.b*7 + C.rgba.a*11)
#define QOI_MAGIC \
    (((uint32_t)'q') << 24 | ((uint32_t)'o') << 16 | \
     ((uint32_t)'i') <<  8 | ((uint32_t)'f'))
#define QOI_HEADER_SIZE 14
#define QOI_PADDING_SIZE 8

/* 2GB is the max file size that this implementation can safely handle. We guard
against anything larger than that, assuming the worst case with 5 bytes per
pixel, rounded down to a nice clean value. 400 million pixels ought to be
enough for anybody. */
#define QOI_PIXELS_MAX ((uint32_t)400000000)

union qoi_rgba_t {
    struct { uint8_t r, g, b, a; } rgba;
    argb_t v;
};

static uint32_t qoi_read_32(const uint8_t *bytes, int *p) {
    uint32_t a = bytes[(*p)++];
    uint32_t b = bytes[(*p)++];
    uint32_t c = bytes[(*p)++];
    uint32_t d = bytes[(*p)++];
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
    argb_t *pixels;
    union qoi_rgba_t index[64];
    union qoi_rgba_t px;
    int px_len, chunks_len, px_pos;
    int p = 0, run = 0;

    if (size < QOI_HEADER_SIZE + QOI_PADDING_SIZE) {
        return ldr_fmterror;
    }

    header_magic = qoi_read_32(bytes, &p);
    width = qoi_read_32(bytes, &p);
    height = qoi_read_32(bytes, &p);
    channels = bytes[p++];
    colorspace = bytes[p++];

    if (
        width == 0 || height == 0 ||
        channels < 3 || channels > 4 ||
        colorspace > 1 ||
        header_magic != QOI_MAGIC ||
        height >= QOI_PIXELS_MAX / width
    ) {
        return ldr_fmterror;
    }

    image_set_format(ctx, "QOI %dchannels", channels);
    channels = 4;

    if (!image_allocate_frame(ctx, width, height)) {
        return ldr_fmterror;
    }
    ctx->alpha = channels == 4;

    pixels = ctx->frames[0].pm.data;

    memset(index, 0, sizeof(index));
    px.rgba.r = 0;
    px.rgba.g = 0;
    px.rgba.b = 0;
    px.rgba.a = 255;

    px_len = width * height;
    chunks_len = size - QOI_PADDING_SIZE;
    for (px_pos = 0; px_pos < px_len; px_pos += 1) {
        if (run > 0) {
            run--;
        }
        else if (p < chunks_len) {
            int b1 = bytes[p++];
            if (b1 == QOI_OP_RGB) {
                px.rgba.r = bytes[p++];
                px.rgba.g = bytes[p++];
                px.rgba.b = bytes[p++];
            }
            else if (b1 == QOI_OP_RGBA) {
                px.rgba.r = bytes[p++];
                px.rgba.g = bytes[p++];
                px.rgba.b = bytes[p++];
                px.rgba.a = bytes[p++];
            }
            else if ((b1 & QOI_MASK_2) == QOI_OP_INDEX) {
                px = index[b1];
            }
            else if ((b1 & QOI_MASK_2) == QOI_OP_DIFF) {
                px.rgba.r += ((b1 >> 4) & 0x03) - 2;
                px.rgba.g += ((b1 >> 2) & 0x03) - 2;
                px.rgba.b += ( b1       & 0x03) - 2;
            }
            else if ((b1 & QOI_MASK_2) == QOI_OP_LUMA) {
                int b2 = bytes[p++];
                int vg = (b1 & 0x3f) - 32;
                px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
                px.rgba.g += vg;
                px.rgba.b += vg - 8 +  (b2       & 0x0f);
            }
            else if ((b1 & QOI_MASK_2) == QOI_OP_RUN) {
                run = (b1 & 0x3f);
            }
            index[QOI_COLOR_HASH(px) % 64] = px;
        }
        pixels[px_pos] = px.v;
    }
    return ldr_success;
}
