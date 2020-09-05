// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

//
// GIF image format support
//

#include "config.h"
#ifndef HAVE_LIBGIF
#error Invalid build configuration
#endif

#include "image.h"

#include <stdlib.h>
#include <string.h>

#include <gif_lib.h>

// Format name
static const char* format_name = "GIF";

// GIF signature
static const uint8_t signature[] = { 'G', 'I', 'F' };

cairo_surface_t* load_gif(const char* file, const uint8_t* header)
{
    cairo_surface_t* img = NULL;

    // check signature
    if (memcmp(header, signature, sizeof(signature))) {
        return NULL;
    }

    int err;
    GifFileType* gif = DGifOpenFileName(file, &err);
    if (!gif) {
        log_error(format_name, 0, "[%i] %s", err, GifErrorString(err));
        return NULL;
    }

    // decode with high-level API
    if (DGifSlurp(gif) != GIF_OK) {
        log_error(format_name, 0, "Decoder error: %s", GifErrorString(gif->Error));
        goto done;
    }
    if (!gif->SavedImages) {
        log_error(format_name, 0, "No saved images");
        goto done;
    }

    // create canvas
    img = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, gif->SWidth, gif->SHeight);
    if (cairo_surface_status(img) != CAIRO_STATUS_SUCCESS) {
        log_error(format_name, 0, "Unable to create surface: %s",
                  cairo_status_to_string(cairo_surface_status(img)));
        cairo_surface_destroy(img);
        img = NULL;
        goto done;
    }

    // we don't support animation, show the first frame only
    const GifImageDesc* frame = &gif->SavedImages->ImageDesc;
    const GifColorType* colors = gif->SColorMap ? gif->SColorMap->Colors :
                                                  frame->ColorMap->Colors;
    uint32_t* base = (uint32_t*)(cairo_image_surface_get_data(img) +
                     frame->Top * cairo_image_surface_get_stride(img));
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

    cairo_surface_mark_dirty(img);
    cairo_surface_set_user_data(img, &meta_fmt_name, (void*)format_name, NULL);

done:
    DGifCloseFile(gif, NULL);
    return img;
}
