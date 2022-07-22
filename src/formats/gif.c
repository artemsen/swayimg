// SPDX-License-Identifier: MIT
// GIF format decoder.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include <gif_lib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
bool load_gif(image_t* img, const uint8_t* data, size_t size)
{
    // check signature
    if (size < sizeof(signature) ||
        memcmp(data, signature, sizeof(signature))) {
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
        image_error(img, "unable to open gif decoder: [%d] %s", err,
                    GifErrorString(err));
        return false;
    }

    // decode with high-level API
    if (DGifSlurp(gif) != GIF_OK) {
        image_error(img, "unable to decode gif image: [%d] %s", err,
                    GifErrorString(err));
        DGifCloseFile(gif, NULL);
        return false;
    }
    if (!gif->SavedImages) {
        image_error(img, "gif doesn't contain images");
        DGifCloseFile(gif, NULL);
        return false;
    }

    if (!image_allocate(img, gif->SWidth, gif->SHeight)) {
        DGifCloseFile(gif, NULL);
        return false;
    }

    // we don't support animation, show the first frame only
    const GifImageDesc* frame = &gif->SavedImages->ImageDesc;
    const GifColorType* colors =
        gif->SColorMap ? gif->SColorMap->Colors : frame->ColorMap->Colors;
    uint32_t* base = &img->data[frame->Top * img->width];
    for (int y = 0; y < frame->Height; ++y) {
        uint32_t* pixel = base + y * gif->SWidth + frame->Left;
        const uint8_t* raster = &gif->SavedImages->RasterBits[y * gif->SWidth];
        for (int x = 0; x < frame->Width; ++x) {
            const uint8_t color = raster[x];
            if (color != gif->SBackGroundColor) {
                const GifColorType* rgb = &colors[color];
                *pixel =
                    (0xff << 24) | rgb->Red << 16 | rgb->Green << 8 | rgb->Blue;
            }
            ++pixel;
        }
    }

    add_image_info(img, "Format", "GIF, frame 1 of %d", gif->ImageCount);
    img->alpha = true;

    DGifCloseFile(gif, NULL);

    return true;
}
