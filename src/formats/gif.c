// SPDX-License-Identifier: MIT
// GIF format decoder.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include <gif_lib.h>
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
 * @param img image context
 * @param gif gif context
 * @param index number of the frame to load
 * @return true if completed successfully
 */
static bool decode_frame(struct image* img, GifFileType* gif, size_t index)
{
    const SavedImage* gif_img = &gif->SavedImages[index];
    const GifImageDesc* desc = &gif_img->ImageDesc;
    const ColorMapObject* color_map =
        desc->ColorMap ? desc->ColorMap : gif->SColorMap;
    GraphicsControlBlock ctl = { .TransparentColor = NO_TRANSPARENT_COLOR };
    struct image_frame* frame = &img->frames[index];

    const size_t width = (size_t)desc->Width > frame->pm.width - desc->Left
        ? frame->pm.width - desc->Left
        : (size_t)desc->Width;
    const size_t height = (size_t)desc->Height > frame->pm.height - desc->Top
        ? frame->pm.height - desc->Top
        : (size_t)desc->Height;

    DGifSavedExtensionToGCB(gif, index, &ctl);

    if (ctl.DisposalMode == DISPOSE_PREVIOUS && index < img->num_frames - 1) {
        struct pixmap* next = &img->frames[index + 1].pm;
        pixmap_copy(&frame->pm, next, 0, 0, false);
    }

    for (size_t y = 0; y < height; ++y) {
        const uint8_t* raster = &gif_img->RasterBits[y * desc->Width];
        argb_t* pixel = frame->pm.data + desc->Top * frame->pm.width +
            y * frame->pm.width + desc->Left;

        for (size_t x = 0; x < width; ++x) {
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

    if (ctl.DisposalMode == DISPOSE_DO_NOT && index < img->num_frames - 1) {
        struct pixmap* next = &img->frames[index + 1].pm;
        pixmap_copy(&frame->pm, next, 0, 0, false);
    }

    if (ctl.DelayTime != 0) {
        frame->duration = ctl.DelayTime * 10; // hundreds of second to ms
    } else {
        frame->duration = 100;
    }

    return true;
}

//  GIF loader implementation
enum image_status decode_gif(struct image* img, const uint8_t* data,
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
        return imgload_unsupported;
    }

    // decode
    gif = DGifOpen(&buf, gif_reader, &err);
    if (!gif) {
        return imgload_fmterror;
    }
    if (DGifSlurp(gif) != GIF_OK) {
        goto fail;
    }

    // allocate frame sequence
    if (!image_alloc_frames(img, gif->ImageCount)) {
        goto fail;
    }
    for (size_t i = 0; i < img->num_frames; ++i) {
        struct pixmap* pm = &img->frames[i].pm;
        if (!pixmap_create(pm, gif->SWidth, gif->SHeight)) {
            goto fail;
        }
    }

    // decode every frame
    for (size_t i = 0; i < img->num_frames; ++i) {
        if (!decode_frame(img, gif, i)) {
            goto fail;
        }
    }

    image_set_format(img, "GIF%s", gif->ImageCount > 1 ? " animation" : "");
    img->alpha = true;

    DGifCloseFile(gif, NULL);

    return imgload_success;

fail:
    DGifCloseFile(gif, NULL);
    image_free(img, IMGFREE_FRAMES);
    return imgload_fmterror;
}
