// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

//
// SVG image format support
//

#include "config.h"
#ifndef HAVE_LIBRSVG
#error Invalid build configuration
#endif

#include "loader.h"

#include <string.h>
#include <librsvg/rsvg.h>

// Format name
static const char* const format_name = "SVG";

// SVG signature
static const uint8_t signature[] = { '<' };

// implementation of struct loader::load
static cairo_surface_t* load(const char* file, const uint8_t* header, size_t header_len)
{
    RsvgHandle* svg;
    RsvgDimensionData dim;
    GError* err = NULL;
    cairo_t* cr;
    cairo_surface_t* img = NULL;

    // check signature
    if (header_len < sizeof(signature) || memcmp(header, signature, sizeof(signature))) {
        return NULL;
    }

    svg = rsvg_handle_new_from_file(file, &err);
    if (!svg) {
        if (err && err->message) {
            load_error(format_name, 0, "Decode error: %s", err->message);
        } else {
            load_error(format_name, 0, "Something went wrong");
        }
        return NULL;
    }

    rsvg_handle_get_dimensions(svg, &dim);

    // create surface
    img = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, dim.width, dim.height);
    if (cairo_surface_status(img) != CAIRO_STATUS_SUCCESS) {
        load_error(format_name, 0, "Unable to create surface: %s",
                   cairo_status_to_string(cairo_surface_status(img)));
        cairo_surface_destroy(img);
        g_object_unref(svg);
        return NULL;
    }

    // render svg to surface
    cr = cairo_create(img);
    rsvg_handle_render_cairo(svg, cr);
    cairo_destroy(cr);

    g_object_unref(svg);

    return img;
}

// declare format
const struct loader svg_loader = {
    .format = format_name,
    .load = load
};
