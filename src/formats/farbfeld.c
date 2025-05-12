// SPDX-License-Identifier: MIT
// Farbfeld format decoder

#include "loader.h"

#include <arpa/inet.h>
#include <string.h>

// Farbfeld signature
static const uint8_t signature[] = { 'f', 'a', 'r', 'b', 'f', 'e', 'l', 'd' };

// Farbfeld file header
struct __attribute__((__packed__)) farbfeld_header {
    uint8_t magic[sizeof(signature)];
    uint32_t width;
    uint32_t height;
};

// Packed Farbfeld pixel
struct __attribute__((__packed__)) farbfeld_rgba {
    uint16_t r;
    uint16_t g;
    uint16_t b;
    uint16_t a;
};

// Farbfeld loader implementation
enum image_status decode_farbfeld(struct imgdata* img, const uint8_t* data,
                                  size_t size)
{
    const struct farbfeld_header* header = (const struct farbfeld_header*)data;
    size_t width, height, total;
    struct farbfeld_rgba* src;
    argb_t* dst;
    struct pixmap* pm;

    // check signature
    if (size < sizeof(*header) ||
        memcmp(header->magic, signature, sizeof(signature))) {
        return imgload_unsupported;
    }

    // create pixmap
    width = htonl(header->width);
    height = htonl(header->height);
    pm = image_alloc_frame(img, width, height);
    if (!pm) {
        return imgload_fmterror;
    }

    size -= sizeof(struct farbfeld_header);
    data += sizeof(struct farbfeld_header);

    // decode image
    dst = pm->data;
    src = (struct farbfeld_rgba*)data;
    total = min(width * height, size / sizeof(struct farbfeld_rgba));
    for (size_t i = 0; i < total; ++i) {
        *dst = ARGB(src->a, src->r, src->g, src->b);
        ++dst;
        ++src;
    }

    image_set_format(img, "Farbfeld");
    img->alpha = true;

    return imgload_success;
}
