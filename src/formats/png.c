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

// PNG loader implementation
struct image* load_png(const char* file, const uint8_t* header, size_t header_len)
{
    struct image* img = NULL;

    // check signature
    if (header_len < sizeof(signature) || memcmp(header, signature, sizeof(signature))) {
        return NULL;
    }

    img = calloc(1, sizeof(struct image));
    if (!img) {
        fprintf(stderr, "Not enough memory\n");
        return NULL;
    }

    // load png image via Cairo toy API
    img->surface = cairo_image_surface_create_from_png(file);
    const cairo_status_t status = cairo_surface_status(img->surface);
    if (status != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "PNG decode failed: %s\n",
                cairo_status_to_string(status));
        free_image(img);
        return NULL;
    }

    img->format = malloc(4);
    if (!img->format) {
        fprintf(stderr, "Not enough memory\n");
        free_image(img);
        return NULL;
    }
    strcpy((char*)img->format, "PNG");

    return img;
}
