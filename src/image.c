// SPDX-License-Identifier: MIT
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "image.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

struct image* create_image(cairo_format_t color, size_t width, size_t height)
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

    return img;
}

void set_image_meta(struct image* img, const char* format, ...)
{
    int len;

    va_list args;
    va_start(args, format);
    len = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (len < 0) {
        fprintf(stderr, "Invalid format description\n");
        free_image(img);
        return;
    }

    len += 1; // including the terminating null
    img->format = malloc(len);
    if (!img->format) {
        fprintf(stderr, "Not enough memory\n");
        free_image(img);
        return;
    }

    va_start(args, format);
    vsnprintf((char*)img->format, len, format, args);
    va_end(args);
}

void free_image(struct image* img)
{
    if (img) {
#ifdef HAVE_LIBEXIF
        if (img->exif) {
            exif_data_unref(img->exif);
        }
#endif // HAVE_LIBEXIF
        if (img->format) {
            free((void*)img->format);
        }
        if (img->surface) {
            cairo_surface_destroy(img->surface);
        }
        free(img);
    }
}
