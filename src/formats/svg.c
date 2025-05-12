// SPDX-License-Identifier: MIT
// SVG format decoder.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include <ctype.h>
#include <string.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wexpansion-to-defined"
#include <librsvg/rsvg.h>
#pragma GCC diagnostic pop

// SVG uses physical units to store size,
// these macro defines default viewbox dimension in pixels
#define RENDER_SIZE 1024

// Max offset of the root svg node in xml file
#define MAX_OFFSET 1024

/**
 * Check if data is SVG.
 * @param data raw image data
 * @param size size of image data in bytes
 * @return true if it is an SVG format
 */
static bool is_svg(const uint8_t* data, size_t size)
{
    const char svg_node[] = { '<', 's', 'v', 'g' };
    size_t max_pos;

    if (size < sizeof(svg_node)) {
        return false;
    }

    max_pos = size - sizeof(svg_node);
    if (max_pos > MAX_OFFSET) {
        max_pos = MAX_OFFSET;
    }

    // search for svg node
    for (size_t i = 0; i < max_pos; ++i) {
        if (memcmp(data + i, svg_node, sizeof(svg_node)) == 0) {
            return true;
        }
    }

    return false;
}

// SVG loader implementation
enum image_status decode_svg(struct imgdata* img, const uint8_t* data,
                             size_t size)
{
    RsvgHandle* svg;
    gboolean has_vb_real;
    RsvgRectangle vb_real;
    RsvgRectangle vb_render;
    GError* err = NULL;
    cairo_surface_t* surface = NULL;
    cairo_t* cr = NULL;
    struct pixmap* pm;
    cairo_status_t status;

    if (!is_svg(data, size)) {
        return imgload_unsupported;
    }

    svg = rsvg_handle_new_from_data(data, size, &err);
    if (!svg) {
        return imgload_fmterror;
    }

    // define image size in pixels
    rsvg_handle_get_intrinsic_dimensions(svg, NULL, NULL, NULL, NULL,
                                         &has_vb_real, &vb_real);
    vb_render.x = 0;
    vb_render.y = 0;
    if (has_vb_real) {
        if (vb_real.width < vb_real.height) {
            vb_render.width = RENDER_SIZE * (vb_real.width / vb_real.height);
            vb_render.height = RENDER_SIZE;
        } else {
            vb_render.width = RENDER_SIZE;
            vb_render.height = RENDER_SIZE * (vb_real.height / vb_real.width);
        }
    } else {
        vb_render.width = RENDER_SIZE;
        vb_render.height = RENDER_SIZE;
    }

    // allocate and bind buffer
    pm = image_alloc_frame(img, vb_render.width, vb_render.height);
    if (!pm) {
        goto fail;
    }
    surface = cairo_image_surface_create_for_data(
        (uint8_t*)pm->data, CAIRO_FORMAT_ARGB32, pm->width, pm->height,
        pm->width * sizeof(argb_t));
    status = cairo_surface_status(surface);
    if (status != CAIRO_STATUS_SUCCESS) {
        goto fail;
    }

    // render svg to surface
    cr = cairo_create(surface);
    if (!rsvg_handle_render_document(svg, cr, &vb_render, &err)) {
        goto fail;
    }

    image_set_format(img, "SVG");
    if (has_vb_real) {
        image_add_info(img, "Real size", "%0.2fx%0.2f", vb_real.width,
                       vb_real.height);
    }
    img->alpha = true;

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    g_object_unref(svg);

    return imgload_success;

fail:
    if (cr) {
        cairo_destroy(cr);
    }
    if (surface) {
        cairo_surface_destroy(surface);
    }
    g_object_unref(svg);
    return imgload_fmterror;
}
