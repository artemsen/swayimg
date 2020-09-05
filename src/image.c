// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "config.h"
#include "image.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

// meta data
const cairo_user_data_key_t meta_fmt_name;

// loaders declaration
cairo_surface_t* load_png(const char* file, const uint8_t* header);
cairo_surface_t* load_bmp(const char* file, const uint8_t* header);
#ifdef HAVE_LIBJPEG
cairo_surface_t* load_jpeg(const char* file, const uint8_t* header);
#endif
#ifdef HAVE_LIBGIF
cairo_surface_t* load_gif(const char* file, const uint8_t* header);
#endif
#ifdef HAVE_LIBRSVG
cairo_surface_t* load_svg(const char* file, const uint8_t* header);
#endif
#ifdef HAVE_LIBWEBP
cairo_surface_t* load_webp(const char* file, const uint8_t* header);
#endif

// ordered list of available loaders
static const load loaders[] = {
    load_png,
#ifdef HAVE_LIBJPEG
    load_jpeg,
#endif
#ifdef HAVE_LIBGIF
    load_gif,
#endif
#ifdef HAVE_LIBRSVG
    load_svg,
#endif
#ifdef HAVE_LIBWEBP
    load_webp,
#endif
    load_bmp,
};

cairo_surface_t* load_image(const char* file)
{
    cairo_surface_t* img = NULL;

    // read header
    uint8_t header[HEADER_SIZE];
    const int fd = open(file, O_RDONLY);
    if (fd == -1) {
        const int ec = errno;
        fprintf(stderr, "Unable to open file %s: [%i] %s\n", file, ec, strerror(ec));
        return NULL;
    }
    if (read(fd, header, sizeof(header)) != sizeof(header)) {
        const int ec = errno ? errno : ENODATA;
        fprintf(stderr, "Unable to read file %s: [%i] %s\n", file, ec, strerror(ec));
        close(fd);
        return NULL;
    }
    close(fd);

    // try to decode
    for (size_t i = 0; !img && i < sizeof(loaders) / sizeof(loaders[0]); ++i) {
        img = loaders[i](file, header);
    }

    if (!img) {
        fprintf(stderr, "Unsupported format: %s\n", file);
    }

    return img;
}

void log_error(const char* name, int errcode, const char* fmt, ...)
{
    fprintf(stderr, "%s: ", name);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    if (errcode) {
        fprintf(stderr, ": [%i] %s", errcode, strerror(errcode));
    }

    fprintf(stderr, "\n");
}
