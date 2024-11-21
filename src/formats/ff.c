// SPDX-License-Identifier: MIT
// farbfeld format decoder

#include "../loader.h"

#include <arpa/inet.h>
#include <string.h>

#define MAGIC_BYTES_LEN 8
#define MAGIC_BYTES     "farbfeld"
#define PIXEL_BYTES     8
#define CHANNEL_BYTES   2
#define R_POS           0
#define G_POS           2
#define B_POS           4
#define A_POS           6

struct farbfeld_header {
    uint8_t magic[MAGIC_BYTES_LEN];
    uint32_t width;
    uint32_t height;
};

uint32_t ff_size(struct farbfeld_header header)
{
    return sizeof(struct farbfeld_header) +
        header.width * header.height * sizeof(argb_t) * CHANNEL_BYTES;
}

enum loader_status decode_ff(struct image* ctx, const uint8_t* data,
                             size_t size)
{

    if (size < sizeof(struct farbfeld_header)) {
        return ldr_unsupported;
    }

    if (memcmp(data, MAGIC_BYTES, MAGIC_BYTES_LEN) != 0) {
        return ldr_unsupported;
    }

    uint32_t width = *(const uint32_t*)&data[MAGIC_BYTES_LEN];
    width = htonl(width);
    uint32_t height = *(const uint32_t*)&data[MAGIC_BYTES_LEN + sizeof(width)];
    height = htonl(height);

    const struct farbfeld_header header = { MAGIC_BYTES, width, height };

    if (ff_size(header) > size) {
        return ldr_fmterror;
    }

    ctx->alpha = true;

    if (!image_allocate_frame(ctx, header.width, header.height)) {
        return ldr_fmterror;
    }

    struct pixmap* pm = &ctx->frames[0].pm;

    size_t imax = min(size, ff_size(header));
    for (size_t i = sizeof(struct farbfeld_header); i < imax;
         i = i + PIXEL_BYTES) {
        argb_t pixel = ARGB(data[i + A_POS], data[i + R_POS], data[i + G_POS],
                            data[i + B_POS]);
        pm->data[(i - sizeof(struct farbfeld_header)) / PIXEL_BYTES] = pixel;
    }

    image_set_format(ctx, "farbfeld");

    return ldr_success;
}
