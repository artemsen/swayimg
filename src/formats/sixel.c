// SPDX-License-Identifier: MIT
// Sixel format decoder.

#include "../loader.h"

#include <sixel.h>
#include <stdio.h>
#include <stdlib.h>

// Sixel loader implementation
enum loader_status decode_sixel(struct image* ctx, const uint8_t* data,
                                size_t size)
{
    uint8_t* pixels = NULL;
    uint8_t* palette = NULL;
    int width, height, ncolors;
    SIXELSTATUS status;

    // sixel always starts with Esc code
    if (data[0] != 0x1b) {
        return ldr_unsupported;
    }

    // decode image
    status = sixel_decode_raw((uint8_t*)data, (int)size, &pixels, &width,
                              &height, &palette, &ncolors, NULL);
    if (SIXEL_FAILED(status)) {
        return ldr_unsupported;
    }

    if (!image_allocate_frame(ctx, width, height)) {
        free(pixels);
        free(palette);
        return ldr_fmterror;
    }

    // convert palette to real pixels
    for (int y = 0; y < height; ++y) {
        const int y_offset = y * width;
        const uint8_t* src = &pixels[y_offset];
        argb_t* dst = &ctx->frames[0].pm.data[y_offset];
        for (int x = 0; x < width; ++x) {
            if (src[x] >= ncolors) {
                dst[x] = ARGB(0xff, 0, 0, 0);
            } else {
                const uint8_t* rgb = &palette[src[x] * 3];
                dst[x] = ARGB(0xff, rgb[0], rgb[1], rgb[2]);
            }
        }
    }

    image_set_format(ctx, "Sixel");

    free(pixels);
    free(palette);
    return ldr_success;
}
