// SPDX-License-Identifier: MIT
// Image loader: interface and common framework for decoding images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include "buildcfg.h"

// Construct function name of loader
#define LOADER_FUNCTION(name) decode_##name
// Declaration of loader function
#define LOADER_DECLARE(name)                                     \
    enum loader_status LOADER_FUNCTION(name)(struct image * ctx, \
                                             const uint8_t* data, size_t size)

const char* supported_formats = "bmp, pnm"
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
#ifdef HAVE_LIBHEIF
                                ", heif, avif"
#endif
#ifdef HAVE_LIBJXL
                                ", jxl"
#endif
#ifdef HAVE_LIBTIFF
                                ", tiff"
#endif
    ;

// declaration of loaders
LOADER_DECLARE(bmp);
LOADER_DECLARE(pnm);
#ifdef HAVE_LIBGIF
LOADER_DECLARE(gif);
#endif
#ifdef HAVE_LIBHEIF
LOADER_DECLARE(heif);
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
#ifdef HAVE_LIBTIFF
LOADER_DECLARE(tiff);
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
    &LOADER_FUNCTION(pnm),
#ifdef HAVE_LIBWEBP
    &LOADER_FUNCTION(webp),
#endif
#ifdef HAVE_LIBHEIF
    &LOADER_FUNCTION(heif),
#endif
#ifdef HAVE_LIBRSVG
    &LOADER_FUNCTION(svg),
#endif
#ifdef HAVE_LIBJXL
    &LOADER_FUNCTION(jxl),
#endif
#ifdef HAVE_LIBTIFF
    &LOADER_FUNCTION(tiff),
#endif
};

enum loader_status load_image(struct image* ctx, const uint8_t* data,
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
