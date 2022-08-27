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

/**
 * Decode single GIF frame.
 * @param gif gif context
 * @param index number of the frame to load
 * @param prev previous frame
 * @param curr frame to store the image
 * @return true if completed successfully
 */
static bool decode_frame(GifFileType* gif, size_t index,
                         const struct image_frame* prev,
                         struct image_frame* curr)
{
    const SavedImage* gif_image;
    const GifImageDesc* gif_desc;
    const ColorMapObject* gif_colors;
    GraphicsControlBlock gif_cb;
    int color_transparent = NO_TRANSPARENT_COLOR;

    if (!image_frame_allocate(curr, gif->SWidth, gif->SHeight)) {
        return false;
    }
    if (prev) {
        memcpy(curr->data, prev->data,
               curr->width * curr->height * sizeof(argb_t));
    } else {
        memset(curr->data, 0, curr->width * curr->height * sizeof(argb_t));
    }

    if (DGifSavedExtensionToGCB(gif, index, &gif_cb) == GIF_OK) {
        color_transparent = gif_cb.TransparentColor;
        if (gif_cb.DelayTime != 0) {
            curr->duration = gif_cb.DelayTime * 10; // hundreds of second to ms
        }
    }

    gif_image = &gif->SavedImages[index];
    gif_desc = &gif_image->ImageDesc;
    gif_colors = gif_desc->ColorMap ? gif_desc->ColorMap : gif->SColorMap;
    for (int y = 0; y < gif_desc->Height; ++y) {
        const uint8_t* gif_raster = &gif_image->RasterBits[y * gif_desc->Width];
        argb_t* pixel = curr->data + gif_desc->Top * curr->width +
            y * curr->width + gif_desc->Left;

        for (int x = 0; x < gif_desc->Width; ++x) {
            const uint8_t color = gif_raster[x];
            if (color != color_transparent && color < gif_colors->ColorCount) {
                const GifColorType* rgb = &gif_colors->Colors[color];
                *pixel = ARGB_FROM_A(0xff) | ARGB_FROM_R(rgb->Red) |
                    ARGB_FROM_G(rgb->Green) | ARGB_FROM_B(rgb->Blue);
            }
            ++pixel;
        }
    }

    if (curr->duration == 0) {
        curr->duration = 100; // ms
    }

    return true;
}

//  GIF loader implementation
enum loader_status decode_gif(struct image* ctx, const uint8_t* data,
                              size_t size)
{
    GifFileType* gif = NULL;
    struct buffer buf = {
        .data = data,
        .size = size,
        .position = 0,
    };
    int err;

    // check signature
    if (size < sizeof(signature) ||
        memcmp(data, signature, sizeof(signature))) {
        return ldr_unsupported;
    }

    // decode
    gif = DGifOpen(&buf, gif_reader, &err);
    if (!gif) {
        image_print_error(ctx, "unable to open gif decoder: [%d] %s", err,
                          GifErrorString(err));
        return ldr_fmterror;
    }
    if (DGifSlurp(gif) != GIF_OK) {
        image_print_error(ctx, "unable to decode gif image: [%d] %s", err,
                          GifErrorString(err));
        goto fail;
    }

    // allocate frame sequence
    if (!image_create_frames(ctx, gif->ImageCount)) {
        goto fail;
    }

    // decode every frame
    for (size_t i = 0; i < ctx->num_frames; ++i) {
        const struct image_frame* prev = i ? &ctx->frames[i - 1] : NULL;
        if (!decode_frame(gif, i, prev, &ctx->frames[i])) {
            goto fail;
        }
    }

    image_set_format(ctx, "GIF%s", gif->ImageCount > 1 ? " animation" : "");
    ctx->alpha = true;

    DGifCloseFile(gif, NULL);

    return ldr_success;

fail:
    DGifCloseFile(gif, NULL);
    image_free_frames(ctx);
    return ldr_fmterror;
}
