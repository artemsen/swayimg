// SPDX-License-Identifier: MIT
// QOI format decoder.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include <arpa/inet.h>
#include <string.h>

// Chunk tags
#define QOI_OP_INDEX 0x00
#define QOI_OP_DIFF  0x40
#define QOI_OP_LUMA  0x80
#define QOI_OP_RUN   0xc0
#define QOI_OP_RGB   0xfe
#define QOI_OP_RGBA  0xff

// Mask of second byte in stream
#define QOI_MASK_2 0xc0

// Size of color map
#define QOI_CLRMAP_SIZE 64
// Calc color index in map
#define QOI_CLRMAP_INDEX(r, g, b, a) \
    ((r * 3 + g * 5 + b * 7 + a * 11) % QOI_CLRMAP_SIZE)

// QOI signature
static const uint8_t signature[] = { 'q', 'o', 'i', 'f' };

// QOI file header
struct __attribute__((__packed__)) qoi_header {
    uint8_t magic[4];   // Magic bytes "qoif"
    uint32_t width;     // Image width in pixels
    uint32_t height;    // Image height in pixels
    uint8_t channels;   // Number of color channels: 3 = RGB, 4 = RGBA
    uint8_t colorspace; // 0 = sRGB with linear alpha, 1 = all channels linear
};

// QOI loader implementation
enum image_status decode_qoi(struct imgdata* img, const uint8_t* data,
                             size_t size)
{
    const struct qoi_header* qoi = (const struct qoi_header*)data;
    argb_t color_map[QOI_CLRMAP_SIZE];
    struct pixmap* pm;
    uint8_t a, r, g, b;
    size_t total_pixels;
    size_t rlen;
    size_t pos;

    // check signature
    if (size < sizeof(*qoi) ||
        memcmp(qoi->magic, signature, sizeof(signature))) {
        return imgload_unsupported;
    }
    // check format
    if (qoi->width == 0 || qoi->height == 0 || qoi->channels < 3 ||
        qoi->channels > 4) {
        return imgload_fmterror;
    }

    // allocate image buffer
    pm = image_alloc_frame(img, qoi->channels == 4 ? pixmap_argb : pixmap_xrgb,
                           htonl(qoi->width), htonl(qoi->height));
    if (!pm) {
        return imgload_fmterror;
    }

    // initialize decoder state
    r = 0;
    g = 0;
    b = 0;
    a = 0xff;
    rlen = 0;
    pos = sizeof(struct qoi_header);
    total_pixels = pm->width * pm->height;
    memset(color_map, 0, sizeof(color_map));

    // decode image
    for (size_t i = 0; i < total_pixels; ++i) {
        if (rlen > 0) {
            --rlen;
        } else {
            uint8_t tag;
            if (pos >= size) {
                break;
            }
            tag = data[pos++];

            if (tag == QOI_OP_RGB) {
                if (pos + 3 >= size) {
                    return imgload_fmterror;
                }
                r = data[pos++];
                g = data[pos++];
                b = data[pos++];
            } else if (tag == QOI_OP_RGBA) {
                if (pos + 4 >= size) {
                    return imgload_fmterror;
                }
                r = data[pos++];
                g = data[pos++];
                b = data[pos++];
                a = data[pos++];
            } else if ((tag & QOI_MASK_2) == QOI_OP_INDEX) {
                const argb_t clr = color_map[tag & 0x3f];
                a = ARGB_GET_A(clr);
                r = ARGB_GET_R(clr);
                g = ARGB_GET_G(clr);
                b = ARGB_GET_B(clr);
            } else if ((tag & QOI_MASK_2) == QOI_OP_DIFF) {
                r += (int8_t)((tag >> 4) & 3) - 2;
                g += (int8_t)((tag >> 2) & 3) - 2;
                b += (int8_t)(tag & 3) - 2;
            } else if ((tag & QOI_MASK_2) == QOI_OP_LUMA) {
                uint8_t diff;
                int8_t diff_green;
                if (pos + 1 >= size) {
                    return imgload_fmterror;
                }
                diff = data[pos++];
                diff_green = (int8_t)(tag & 0x3f) - 32;
                r += diff_green - 8 + ((diff >> 4) & 0x0f);
                g += diff_green;
                b += diff_green - 8 + (diff & 0x0f);
            } else if ((tag & QOI_MASK_2) == QOI_OP_RUN) {
                rlen = (tag & 0x3f);
            }
            color_map[QOI_CLRMAP_INDEX(r, g, b, a)] = ARGB(a, r, g, b);
        }
        pm->data[i] = ARGB(a, r, g, b);
    }

    image_set_format(img, "QOI %dbpp", qoi->channels * 8);
    return imgload_success;
}
