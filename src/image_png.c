// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

//
// PNG image format support
//

#include "image.h"
#include <string.h>

// Format name
static const char* format_name = "PNG";

// PNG signature
static const uint8_t signature[] = { 0x89, 0x50, 0x4E, 0x47,
                                     0x0D, 0x0A, 0x1A, 0x0A };

cairo_surface_t* load_png(const char* file, const uint8_t* header)
{
    // check signature
    if (memcmp(header, signature, sizeof(signature))) {
        return NULL;
    }

    // load png image via Cairo toy API
    cairo_surface_t* img = cairo_image_surface_create_from_png(file);
    cairo_status_t status = cairo_surface_status(img);
    if (status != CAIRO_STATUS_SUCCESS) {
        log_error(format_name, 0, "Decode failed: %s", cairo_status_to_string(status));
        cairo_surface_destroy(img);
        img = NULL;
    }

    return img;
}
