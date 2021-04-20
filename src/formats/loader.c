// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "config.h"
#include "loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

// loaders declaration
extern const struct loader png_loader;
extern const struct loader bmp_loader;
#ifdef HAVE_LIBJPEG
extern const struct loader jpeg_loader;
#endif
#ifdef HAVE_LIBGIF
extern const struct loader gif_loader;
#endif
#ifdef HAVE_LIBRSVG
extern const struct loader svg_loader;
#endif
#ifdef HAVE_LIBWEBP
extern const struct loader webp_loader;
#endif
#ifdef HAVE_LIBAVIF
extern const struct loader avif_loader;
#endif

// list of available loaders
static const struct loader* loaders[] = {
    &png_loader,
    &bmp_loader,
#ifdef HAVE_LIBJPEG
    &jpeg_loader,
#endif
#ifdef HAVE_LIBGIF
    &gif_loader,
#endif
#ifdef HAVE_LIBRSVG
    &svg_loader,
#endif
#ifdef HAVE_LIBWEBP
    &webp_loader,
#endif
#ifdef HAVE_LIBAVIF
    &avif_loader,
#endif
};

bool load_image(const char* file, cairo_surface_t** img, const char** format)
{
    // read header
    uint8_t header[16];
    const int fd = open(file, O_RDONLY);
    if (fd == -1) {
        load_error(NULL, errno, "Unable to open file %s", file);
        return NULL;
    }
    if (read(fd, header, sizeof(header)) != sizeof(header)) {
        load_error(NULL, errno ? errno : ENODATA, "Unable to read file %s", file);
        close(fd);
        return NULL;
    }
    close(fd);

    // try to decode
    *img = NULL;
    for (size_t i = 0; i < sizeof(loaders) / sizeof(loaders[0]); ++i) {
        *img = loaders[i]->load(file, header, sizeof(header));
        if (*img) {
            *format = loaders[i]->format;
            return true;
        }
    }

    load_error(NULL, 0, "Unsupported format: %s\n", file);
    return false;
}

void load_error(const char* name, int errcode, const char* fmt, ...)
{
    if (name) {
        fprintf(stderr, "%s: ", name);
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    if (errcode) {
        fprintf(stderr, ": [%i] %s", errcode, strerror(errcode));
    }

    fprintf(stderr, "\n");
}
