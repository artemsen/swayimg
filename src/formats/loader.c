// SPDX-License-Identifier: MIT
// Image loader: interface and common framework for decoding images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include "buildcfg.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Construct function name of loader
#define LOADER_FUNCTION(name) decode_##name
// Declaration of loader function
#define LOADER_DECLARE(name)                                     \
    enum loader_status LOADER_FUNCTION(name)(struct image * ctx, \
                                             const uint8_t* data, size_t size)

// declaration of loaders
LOADER_DECLARE(bmp);
#ifdef HAVE_LIBAVIF
LOADER_DECLARE(avif);
#endif
#ifdef HAVE_LIBGIF
LOADER_DECLARE(gif);
#endif
#ifdef HAVE_LIBJPEG
LOADER_DECLARE(jpeg);
#endif
#ifdef HAVE_LIBJXL
LOADER_DECLARE(jxl);
#endif
#ifdef HAVE_LIBPNG
LOADER_DECLARE(png);
#endif
#ifdef HAVE_LIBRSVG
LOADER_DECLARE(svg);
#endif
#ifdef HAVE_LIBWEBP
LOADER_DECLARE(webp);
#endif

// list of available decoders
static const image_decoder decoders[] = {
#ifdef HAVE_LIBJPEG
    &LOADER_FUNCTION(jpeg),
#endif
#ifdef HAVE_LIBPNG
    &LOADER_FUNCTION(png),
#endif
#ifdef HAVE_LIBGIF
    &LOADER_FUNCTION(gif),
#endif
    &LOADER_FUNCTION(bmp),
#ifdef HAVE_LIBWEBP
    &LOADER_FUNCTION(webp),
#endif
#ifdef HAVE_LIBRSVG
    &LOADER_FUNCTION(svg),
#endif
#ifdef HAVE_LIBAVIF
    &LOADER_FUNCTION(avif),
#endif
#ifdef HAVE_LIBJXL
    &LOADER_FUNCTION(jxl),
#endif
};

const char* supported_formats(void)
{
    return "bmp"
#ifdef HAVE_LIBJPEG
           ", jpeg"
#endif
#ifdef HAVE_LIBPNG
           ", png"
#endif
#ifdef HAVE_LIBGIF
           ", gif"
#endif
#ifdef HAVE_LIBWEBP
           ", webp"
#endif
#ifdef HAVE_LIBRSVG
           ", svg"
#endif
#ifdef HAVE_LIBAVIF
           ", avif"
#endif
#ifdef HAVE_LIBJXL
           ", jxl"
#endif
        ;
}

enum loader_status image_decode(struct image* ctx, const uint8_t* data,
                                size_t size)
{
    enum loader_status status = ldr_unsupported;

    for (size_t i = 0; i < sizeof(decoders) / sizeof(decoders[0]); ++i) {
        switch (decoders[i](ctx, data, size)) {
            case ldr_success:
                return ldr_success;
            case ldr_unsupported:
                break;
            case ldr_fmterror:
                status = ldr_fmterror;
                break;
        }
    }

    return status;
}

argb_t* image_allocate(struct image* ctx, size_t width, size_t height)
{
    argb_t* data = malloc(width * height * sizeof(argb_t));

    if (data) {
        ctx->width = width;
        ctx->height = height;
        ctx->data = data;
    } else {
        image_error(ctx, "not enough memory");
    }

    return data;
}

void image_deallocate(struct image* ctx)
{
    ctx->width = 0;
    ctx->height = 0;
    free((void*)ctx->data);
    ctx->data = NULL;
}

void image_error(const struct image* ctx, const char* fmt, ...)
{
    va_list args;

    fprintf(stderr, "%s: ", image_file_name(ctx));

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}
