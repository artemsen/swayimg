// SPDX-License-Identifier: MIT
// Farbfeld format decoder

#include "../loader.h"

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

enum loader_status decode_farbfeld(struct image* ctx, const uint8_t* data,
                                   size_t size)
{
    const struct farbfeld_header* header = (const struct farbfeld_header*)data;
    size_t width, height, total;
    struct farbfeld_rgba* src;
    argb_t* dst;

    // check signature
    if (size < sizeof(*header) ||
        memcmp(header->magic, signature, sizeof(signature))) {
        return ldr_unsupported;
    }

    // create pixmap
    width = htonl(header->width);
    height = htonl(header->height);
    if (!image_allocate_frame(ctx, width, height)) {
        return ldr_fmterror;
    }

    size -= sizeof(struct farbfeld_header);
    data += sizeof(struct farbfeld_header);

    // decode image
    dst = ctx->frames[0].pm.data;
    src = (struct farbfeld_rgba*)data;
    total = min(width * height, size / sizeof(struct farbfeld_rgba));
    for (size_t i = 0; i < total; ++i) {
        *dst = ARGB(src->a, src->r, src->g, src->b);
        ++dst;
        ++src;
    }

    image_set_format(ctx, "Farbfeld");
    ctx->alpha = true;

    return ldr_success;
}
