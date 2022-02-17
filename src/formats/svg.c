// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

//
// SVG image format support
//

#include "common.h"

#include <cairo/cairo.h>
#include <ctype.h>
#include <librsvg/rsvg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// SVG uses physical units to store size,
// these macro defines default viewport dimension in pixels
#define VIEWPORT_SIZE 2048

// SVG signature
static const uint8_t signature[] = { '<' };

// SVG loader implementation
cairo_surface_t* load_svg(const uint8_t* data, size_t size, char* format,
                          size_t format_sz)
{
    cairo_surface_t* surface = NULL;
    RsvgHandle* svg;
    GError* err = NULL;
    gboolean has_viewport;
    RsvgRectangle viewport;
    cairo_t* cr = NULL;

    // check signature, this an xml, so skip spaces from the start
    while (size && isspace(*data) != 0) {
        ++data;
        --size;
    }
    if (size < sizeof(signature) ||
        memcmp(data, signature, sizeof(signature))) {
        return NULL;
    }

    svg = rsvg_handle_new_from_data(data, size, &err);
    if (!svg) {
        fprintf(stderr, "Invalid SVG format");
        if (err && err->message) {
            fprintf(stderr, ": %s\n", err->message);
        } else {
            fprintf(stderr, "\n");
        }
        return NULL;
    }

    // define image size in pixels
    rsvg_handle_get_intrinsic_dimensions(svg, NULL, NULL, NULL, NULL,
                                         &has_viewport, &viewport);
    if (!has_viewport) {
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = VIEWPORT_SIZE;
        viewport.height = VIEWPORT_SIZE;
    }

    // prepare surface and metadata
    surface = create_surface(viewport.width, viewport.height, true);
    if (!surface) {
        goto done;
    }
    snprintf(format, format_sz, "SVG");

    // render svg to surface
    cr = cairo_create(surface);
    if (!rsvg_handle_render_document(svg, cr, &viewport, &err)) {
        fprintf(stderr, "Invalid SVG format");
        if (err && err->message) {
            fprintf(stderr, ": %s\n", err->message);
        } else {
            fprintf(stderr, "\n");
        }
        goto done;
    }

done:
    cairo_destroy(cr);
    g_object_unref(svg);

    return surface;
}
