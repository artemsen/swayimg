// SPDX-License-Identifier: MIT
// SVG format decoder.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include <cairo/cairo.h>
#include <ctype.h>
#include <librsvg/rsvg.h>
#include <string.h>

// SVG uses physical units to store size,
// these macro defines default viewport dimension in pixels
#define VIEWPORT_SIZE 2048

// SVG signature
static const uint8_t signature[] = { '<' };

// SVG loader implementation
bool load_svg(image_t* img, const uint8_t* data, size_t size)
{
    RsvgHandle* svg;
    GError* err = NULL;
    gboolean has_viewport;
    RsvgRectangle viewport;
    cairo_surface_t* surface = NULL;
    cairo_t* cr = NULL;
    cairo_status_t status;

    // check signature, this an xml, so skip spaces from the start
    while (size && isspace(*data) != 0) {
        ++data;
        --size;
    }
    if (size < sizeof(signature) ||
        memcmp(data, signature, sizeof(signature))) {
        return false;
    }

    svg = rsvg_handle_new_from_data(data, size, &err);
    if (!svg) {
        image_error(img, "invalid svg format: %s",
                    err && err->message ? err->message : "unknown error");
        return false;
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
    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, viewport.width,
                                         viewport.height);
    status = cairo_surface_status(surface);
    if (status != CAIRO_STATUS_SUCCESS) {
        const char* desc = cairo_status_to_string(status);
        image_error(img, "unable to create svg surface: %s",
                    desc ? desc : "unknown error");
        if (surface) {
            cairo_surface_destroy(surface);
            surface = NULL;
        }
        g_object_unref(svg);
        return false;
    }

    // render svg to surface
    cr = cairo_create(surface);
    if (!rsvg_handle_render_document(svg, cr, &viewport, &err)) {
        image_error(img, "unable to decode svg: %s",
                    err && err->message ? err->message : "unknown error");
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        g_object_unref(svg);
        return false;
    }

    if (!image_allocate(img, viewport.width, viewport.height)) {
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        g_object_unref(svg);
        return false;
    }

    add_image_info(img, "Format", "SVG");
    img->alpha = true;
    memcpy(img->data, cairo_image_surface_get_data(surface),
           img->width * img->height * sizeof(img->data[0]));

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    g_object_unref(svg);

    return surface;
}
