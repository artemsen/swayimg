// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

//
// GIF image format support
//

#include "config.h"
#ifndef HAVE_LIBGIF
#error Invalid build configuration
#endif

#include "../image.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gif_lib.h>

// GIF signature
static const uint8_t signature[] = { 'G', 'I', 'F' };

// Buffer description for GIF reader
struct buffer {
    const uint8_t* data;
    const size_t size;
    size_t position;
};

// GIF reader callback, see `InputFunc` in gif_lib.h
static int gif_reader(GifFileType* gif, GifByteType* dst, int sz)
{
    struct buffer* buf = (struct buffer*)gif->UserData;
    if (sz >= 0 && buf && buf->position + sz <= buf->size) {
        memcpy(dst, buf->data + buf->position, sz);
        buf->position += sz;
        return sz;
    }
    return -1;
}

// GIF loader implementation
struct image* load_gif(const uint8_t* data, size_t size)
{
    struct image* img = NULL;

    // check signature
    if (size < sizeof(signature) || memcmp(data, signature, sizeof(signature))) {
        return NULL;
    }

    struct buffer buf = {
        .data = data,
        .size = size,
        .position = 0,
    };

    int err;
    GifFileType* gif = DGifOpen(&buf, gif_reader, &err);
    if (!gif) {
        fprintf(stderr, "Unable to open GIF decoder: [%i] %s\n", err, GifErrorString(err));
        return NULL;
    }

    // decode with high-level API
    if (DGifSlurp(gif) != GIF_OK) {
        fprintf(stderr, "Unable to decode GIF: [%i] %s\n", err, GifErrorString(err));
        goto done;
    }
    if (!gif->SavedImages) {
        fprintf(stderr, "No saved images in GIF\n");
        goto done;
    }

    // create image instance
    img = create_image(CAIRO_FORMAT_ARGB32, gif->SWidth, gif->SHeight);
    if (!img) {
        goto done;
    }
    set_image_meta(img, "GIF");

    // we don't support animation, show the first frame only
    const GifImageDesc* frame = &gif->SavedImages->ImageDesc;
    const GifColorType* colors = gif->SColorMap ? gif->SColorMap->Colors :
                                                  frame->ColorMap->Colors;
    uint32_t* base = (uint32_t*)(cairo_image_surface_get_data(img->surface) +
                     frame->Top * cairo_image_surface_get_stride(img->surface));
    for (int y = 0; y < frame->Height; ++y) {
        uint32_t* pixel = base + y * gif->SWidth + frame->Left;
        const uint8_t* raster = &gif->SavedImages->RasterBits[y * gif->SWidth];
        for (int x = 0; x < frame->Width; ++x) {
            const uint8_t color = raster[x];
            if (color != gif->SBackGroundColor) {
                const GifColorType* rgb = &colors[color];
                *pixel = 0xff000000 |
                    rgb->Red << 16 | rgb->Green << 8 | rgb->Blue;
            }
            ++pixel;
        }
    }

    cairo_surface_mark_dirty(img->surface);

done:
    DGifCloseFile(gif, NULL);
    return img;
}
