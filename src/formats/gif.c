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
 * @param ctx image context
 * @param gif gif context
 * @param index number of the frame to load
 * @return true if completed successfully
 */
static bool decode_frame(struct image* ctx, GifFileType* gif, size_t index)
{
    const SavedImage* img = &gif->SavedImages[index];
    const GifImageDesc* desc = &img->ImageDesc;
    const ColorMapObject* color_map =
        desc->ColorMap ? desc->ColorMap : gif->SColorMap;
    GraphicsControlBlock ctl = { .TransparentColor = NO_TRANSPARENT_COLOR };
    struct image_frame* frame = &ctx->frames[index];

    DGifSavedExtensionToGCB(gif, index, &ctl);

    for (int y = 0; y < desc->Height; ++y) {
        const uint8_t* raster = &img->RasterBits[y * desc->Width];
        argb_t* pixel = frame->pm.data + desc->Top * frame->pm.width +
            y * frame->pm.width + desc->Left;

        for (int x = 0; x < desc->Width; ++x) {
            const uint8_t color = raster[x];
            if (color != ctl.TransparentColor &&
                color < color_map->ColorCount) {
                const GifColorType* rgb = &color_map->Colors[color];
                *pixel = ARGB_SET_A(0xff) | ARGB_SET_R(rgb->Red) |
                    ARGB_SET_G(rgb->Green) | ARGB_SET_B(rgb->Blue);
            }
            ++pixel;
        }
    }

    if (ctl.DisposalMode == DISPOSE_DO_NOT && index < ctx->num_frames - 1) {
        struct pixmap* next = &ctx->frames[index + 1].pm;
        const size_t sz = next->width * next->height * sizeof(argb_t);
        memcpy(next->data, frame->pm.data, sz);
    }

    if (ctl.DelayTime != 0) {
        frame->duration = ctl.DelayTime * 10; // hundreds of second to ms
    } else {
        frame->duration = 100;
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
        image_print_error(ctx, "unable to decode gif image");
        goto fail;
    }

    // allocate frame sequence
    if (!image_create_frames(ctx, gif->ImageCount)) {
        goto fail;
    }
    for (size_t i = 0; i < ctx->num_frames; ++i) {
        struct pixmap* pm = &ctx->frames[i].pm;
        if (!pixmap_create(pm, gif->SWidth, gif->SHeight)) {
            goto fail;
        }
    }

    // decode every frame
    for (size_t i = 0; i < ctx->num_frames; ++i) {
        if (!decode_frame(ctx, gif, i)) {
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
