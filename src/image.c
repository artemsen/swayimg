// SPDX-License-Identifier: MIT
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "config.h"
#include "image.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

/**
 * Image loader function.
 * @param[in] file path to the image file
 * @param[in] header header data
 * @param[in] header_len length if header data in bytes
 * @return image instance or NULL if decode failed
 */
typedef struct image* (*loader)(const char* file, const uint8_t* header, size_t header_len);

// declaration of loaders
struct image* load_bmp(const char* file, const uint8_t* header, size_t header_len);
struct image* load_png(const char* file, const uint8_t* header, size_t header_len);
#ifdef HAVE_LIBJPEG
struct image* load_jpeg(const char* file, const uint8_t* header, size_t header_len);
#endif
#ifdef HAVE_LIBGIF
struct image* load_gif(const char* file, const uint8_t* header, size_t header_len);
#endif
#ifdef HAVE_LIBRSVG
struct image* load_svg(const char* file, const uint8_t* header, size_t header_len);
#endif
#ifdef HAVE_LIBWEBP
struct image* load_webp(const char* file, const uint8_t* header, size_t header_len);
#endif
#ifdef HAVE_LIBAVIF
struct image* load_avif(const char* file, const uint8_t* header, size_t header_len);
#endif

// list of available loaders (functions from formats/*)
static const loader loaders[] = {
    &load_png,
    &load_bmp,
#ifdef HAVE_LIBJPEG
    &load_jpeg,
#endif
#ifdef HAVE_LIBGIF
    &load_gif,
#endif
#ifdef HAVE_LIBRSVG
    &load_svg,
#endif
#ifdef HAVE_LIBWEBP
    &load_webp,
#endif
#ifdef HAVE_LIBAVIF
    &load_avif,
#endif
};

struct image* load_image(const char* file)
{
    struct image* img = NULL;

    // read header
    uint8_t header[16];
    const int fd = open(file, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Unable to open file %s: %s\n", file, strerror(errno));
        return NULL;
    }
    if (read(fd, header, sizeof(header)) != sizeof(header)) {
        fprintf(stderr, "Unable to read file %s: %s\n", file,
                strerror(errno ? errno : ENODATA));
        close(fd);
        return NULL;
    }
    close(fd);

    // try to decode
    for (size_t i = 0; i < sizeof(loaders) / sizeof(loaders[0]); ++i) {
        img = loaders[i](file, header, sizeof(header));
        if (img) {
            return img;
        }
    }

    fprintf(stderr, "Unsupported file format: %s\n", file);
    return NULL;
}

void free_image(struct image* img)
{
    if (img) {
        if (img->surface) {
            cairo_surface_destroy(img->surface);
        }
        if (img->format) {
            free((void*)img->format);
        }
        free(img);
    }
}

struct image* create_image(cairo_format_t color, size_t width, size_t height,
                           const char* format, ...)
{
    struct image* img = NULL;

    img = calloc(1, sizeof(struct image));
    if (!img) {
        fprintf(stderr, "Not enough memory\n");
        return NULL;
    }

    img->surface = cairo_image_surface_create(color, width, height);
    const cairo_status_t status = cairo_surface_status(img->surface);
    if (status != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Unable to create surface: %s\n",
                cairo_status_to_string(status));
        free_image(img);
        return NULL;
    }

    va_list args;
    va_start(args, format);
    int sz = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (sz < 0) {
        fprintf(stderr, "Invalid format description\n");
        free_image(img);
        return NULL;
    }
    sz += 1; // including the terminating null
    img->format = malloc(sz);
    if (!img->format) {
        fprintf(stderr, "Not enough memory\n");
        free_image(img);
        return NULL;
    }
    va_start(args, format);
    vsnprintf((char*)img->format, sz, format, args);
    va_end(args);

    return img;
}
