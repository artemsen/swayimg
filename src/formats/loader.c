// SPDX-License-Identifier: MIT
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include "buildcfg.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Image loader function.
 * @param[in] img image instance
 * @param[in] data raw image data
 * @param[in] size size of image data in bytes
 * @return true if image was loaded
 */
typedef bool (*image_load)(image_t* img, const uint8_t* data, size_t size);

// Construct function name of loader
#define LOADER_FUNCTION(name) load_##name
// Declaration of loader function
#define LOADER_DECLARE(name) \
    bool LOADER_FUNCTION(name)(image_t * img, const uint8_t* data, size_t size)

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

// list of available loaders (functions from formats/*)
static const image_load loaders[] = {
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

bool image_decode(image_t* img, const uint8_t* data, size_t size)
{
    for (size_t i = 0; i < sizeof(loaders) / sizeof(loaders[0]); ++i) {
        if (loaders[i](img, data, size)) {
            return true;
        }
    }
    return false;
}

bool image_allocate(image_t* img, size_t width, size_t height)
{
    img->width = width;
    img->height = height;
    img->data = malloc(img->width * img->height * sizeof(img->data[0]));
    if (!img->data) {
        image_error(img, "not enough memory");
        return false;
    }
    return true;
}

void image_deallocate(image_t* img)
{
    img->width = 0;
    img->height = 0;
    free(img->data);
    img->data = NULL;
}

void image_error(const image_t* img, const char* fmt, ...)
{
    va_list args;
    const char* name;

    name = strrchr(img->path, '/');
    if (name) {
        ++name; // skip slash
    } else {
        name = img->path; // use full path
    }
    fprintf(stderr, "%s: ", name);

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}

/**
 * Apply alpha to color.
 * @param[in] alpha alpha channel value
 * @param[in] color color value
 * @return color with applied alpha
 */
static inline uint8_t multiply_alpha(uint8_t alpha, uint8_t color)
{
    const uint16_t tmp = (alpha * color) + 0x80;
    return ((tmp + (tmp >> 8)) >> 8);
}

void image_apply_alpha(image_t* img)
{
    for (size_t y = 0; y < img->height; ++y) {
        uint32_t* line = &img->data[y * img->width];
        for (size_t x = 0; x < img->width; ++x) {
            uint32_t* pixel = line + x;
            const uint8_t alpha = *pixel >> 24;
            if (alpha != 0xff) {
                *pixel = alpha << 24 |
                    multiply_alpha(alpha, (*pixel >> 16) & 0xff) << 16 |
                    multiply_alpha(alpha, (*pixel >> 8) & 0xff) << 8 |
                    multiply_alpha(alpha, *pixel & 0xff);
            }
        }
    }
}
