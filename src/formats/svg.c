// SPDX-License-Identifier: MIT
// SVG format decoder.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include <ctype.h>
#include <librsvg/rsvg.h>
#include <string.h>

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
    const char svg_begin[] = "<svg";
    const char xml_begin[] = "<?xml";

    // svg is an xml, skip spaces from the start
    while (size && isspace(*data) != 0) {
        ++data;
        --size;
    }

    if (size > sizeof(svg_begin) &&
        strncmp((const char*)data, svg_begin, sizeof(svg_begin) - 1) == 0) {
        return true;
    }

    if (size > sizeof(xml_begin) &&
        strncmp((const char*)data, xml_begin, sizeof(xml_begin) - 1) == 0) {
        // search for svg node
        size_t pos = sizeof(xml_begin);
        while (pos < MAX_OFFSET && pos + sizeof(svg_begin) < size) {
            if (strncmp((const char*)&data[pos], svg_begin,
                        sizeof(svg_begin) - 1) == 0) {
                return true;
            }
            ++pos;
        }
    }

    return false;
}

// SVG loader implementation
enum loader_status decode_svg(struct image* ctx, const uint8_t* data,
                              size_t size)
{
    RsvgHandle* svg;
    gboolean has_vb_real;
    RsvgRectangle vb_real;
    RsvgRectangle vb_render;
    GError* err = NULL;
    cairo_surface_t* surface = NULL;
    cairo_t* cr = NULL;
    struct image_frame* frame;
    cairo_status_t status;

    if (!is_svg(data, size)) {
        return ldr_unsupported;
    }

    svg = rsvg_handle_new_from_data(data, size, &err);
    if (!svg) {
        image_print_error(ctx, "invalid svg format: %s",
                          err && err->message ? err->message : "unknown error");
        return ldr_fmterror;
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
    frame = image_create_frame(ctx, vb_render.width, vb_render.height);
    if (!frame) {
        goto fail;
    }
    memset(frame->data, 0, frame->width * frame->height * sizeof(argb_t));
    surface = cairo_image_surface_create_for_data(
        (uint8_t*)frame->data, CAIRO_FORMAT_ARGB32, frame->width, frame->height,
        frame->width * sizeof(argb_t));
    status = cairo_surface_status(surface);
    if (status != CAIRO_STATUS_SUCCESS) {
        const char* desc = cairo_status_to_string(status);
        image_print_error(ctx, "unable to create cairo surface: %s",
                          desc ? desc : "unknown error");
        goto fail;
    }

    // render svg to surface
    cr = cairo_create(surface);
    if (!rsvg_handle_render_document(svg, cr, &vb_render, &err)) {
        image_print_error(ctx, "unable to decode svg: %s",
                          err && err->message ? err->message : "unknown error");
        goto fail;
    }

    image_set_format(ctx, "SVG");
    if (has_vb_real) {
        image_add_meta(ctx, "Real size", "%0.2fx%0.2f", vb_real.width,
                       vb_real.height);
    }
    ctx->alpha = true;

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    g_object_unref(svg);

    return ldr_success;

fail:
    if (cr) {
        cairo_destroy(cr);
    }
    if (surface) {
        cairo_surface_destroy(surface);
    }
    image_free_frames(ctx);
    g_object_unref(svg);
    return ldr_fmterror;
}
