// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "config.h"
#include "image.h"
#include "image_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

// loaders declaration
extern const struct loader png_loader;
#ifdef HAVE_LIBJPEG
extern const struct loader jpeg_loader;
#endif
#ifdef HAVE_LIBGIF
extern const struct loader gif_loader;
#endif
#ifdef HAVE_LIBWEBP
extern const struct loader webp_loader;
#endif
#ifdef HAVE_LIBRSVG
extern const struct loader svg_loader;
#endif
extern const struct loader bmp_loader;

// ordered list of available loaders
static const struct loader* loaders[] = {
    &png_loader,
#ifdef HAVE_LIBJPEG
    &jpeg_loader,
#endif
#ifdef HAVE_LIBGIF
    &gif_loader,
#endif
#ifdef HAVE_LIBWEBP
    &webp_loader,
#endif
#ifdef HAVE_LIBRSVG
    &svg_loader,
#endif
    &bmp_loader
};

struct image* load_image(const char* file)
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
    for (size_t i = 0; i < sizeof(loaders) / sizeof(loaders[0]); ++i) {
        cairo_surface_t* cs = loaders[i]->load(file, header, sizeof(header));
        if (cs) {
            struct image* img = malloc(sizeof(struct image));
            if (!img) {
                load_error(NULL, errno, "Memory allocation error");
                cairo_surface_destroy(cs);
                return NULL;
            }
            img->image = cs;
            img->format = loaders[i]->format;
            return img;
        }
    }

    load_error(NULL, 0, "Unsupported format: %s\n", file);
    return NULL;
}

void free_image(struct image* img)
{
    cairo_surface_destroy(img->image);
    free(img);
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
