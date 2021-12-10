// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

//
// PNG image format support
//

#include "../image.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// PNG signature
static const uint8_t signature[] = { 0x89, 0x50, 0x4e, 0x47,
                                     0x0d, 0x0a, 0x1a, 0x0a };

// Buffer description for PNG reader
struct buffer {
    const uint8_t* data;
    const size_t size;
    size_t position;
};

// PNG reader callback, see cairo doc for details
static cairo_status_t png_reader(void* closure, unsigned char* data,
                                 unsigned int length)
{
    struct buffer* buf = (struct buffer*)closure;
    if (buf && buf->position + length <= buf->size) {
        memcpy(data, buf->data + buf->position, length);
        buf->position += length;
        return CAIRO_STATUS_SUCCESS;
    }
    return CAIRO_STATUS_READ_ERROR;
}

// PNG loader implementation
struct image* load_png(const uint8_t* data, size_t size)
{
    // check signature
    if (size < sizeof(signature) ||
        memcmp(data, signature, sizeof(signature))) {
        return NULL;
    }

    struct image* img = calloc(1, sizeof(struct image));
    if (!img) {
        fprintf(stderr, "Not enough memory\n");
        return NULL;
    }

    // load png image via Cairo toy API
    struct buffer buf = {
        .data = data,
        .size = size,
        .position = 0,
    };
    img->surface = cairo_image_surface_create_from_png_stream(png_reader, &buf);
    const cairo_status_t status = cairo_surface_status(img->surface);
    if (status != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "PNG decode failed: %s\n",
                cairo_status_to_string(status));
        free_image(img);
        return NULL;
    }

    img->format = malloc(4 /* "PNG\0" */);
    if (!img->format) {
        fprintf(stderr, "Not enough memory\n");
        free_image(img);
        return NULL;
    }
    strcpy((char*)img->format, "PNG");

    return img;
}
