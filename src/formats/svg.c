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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// SVG uses physical units for size, but we need size in pixels
#define VIRTUAL_SIZE_PX 1000

// Max offset of the root svg node in xml file
#define MAX_OFFSET 1024

/** SVG specific decoder data */
struct svg_data {
    RsvgHandle* rsvg_handle; ///< RSVG handle containing the image data
    size_t rotation;         ///< rotation in °
    bool flip_vertical;      ///< whether to flip the image vertically
    bool flip_horizontal;    ///< whether to flip the image horizontally
};

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

/**
 * Rotate the SVG (update internal rotation variable)
 * @param img image container
 * @param angle rotation angle (only 90, 180, or 270)
 */
static void svg_rotate(struct imgdata* img, size_t angle)
{
    struct svg_data* data = (struct svg_data*)img->decoder.data;
    struct pixmap* pm = &((struct imgframe*)arr_nth(img->frames, 0))->pm;

    data->rotation += angle;
    data->rotation %= 360;

    pixmap_rotate(pm, angle);
}

/**
 * Flip the SVG (set internal flip flag)
 * @param img image container
 * @param vertical true for vertical flip, false for horizontal
 */
static void svg_flip(struct imgdata* img, bool vertical)
{
    struct svg_data* data = (struct svg_data*)img->decoder.data;
    struct pixmap* pm = &((struct imgframe*)arr_nth(img->frames, 0))->pm;

    if (vertical) {
        data->flip_vertical = !data->flip_vertical;
        pixmap_flip_vertical(pm);
    } else {
        data->flip_horizontal = !data->flip_horizontal;
        pixmap_flip_horizontal(pm);
    }
}

/** Custom SVG renderer, see `struct image::decoder::renderer`. */
static void svg_render(struct imgdata* img, double scale, ssize_t x, ssize_t y,
                       struct pixmap* dst)
{
    const struct pixmap* pm = &((struct imgframe*)arr_nth(img->frames, 0))->pm;
    const RsvgRectangle viewbox = {
        .x = x,
        .y = y,
        .width = scale * pm->width,
        .height = scale * pm->height,
    };
    struct svg_data* data = img->decoder.data;
    cairo_surface_t* surface;

    // render svg to cairo surface
    surface = cairo_image_surface_create_for_data(
        (uint8_t*)dst->data, CAIRO_FORMAT_ARGB32, dst->width, dst->height,
        dst->width * sizeof(argb_t));
    if (cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS) {
        cairo_t* cairo = cairo_create(surface);

        if (cairo_status(cairo) == CAIRO_STATUS_SUCCESS) {

            // apply rotation / flip
            cairo_translate(cairo, viewbox.width / 2 + x,
                            viewbox.height / 2 + y);
            if (data->rotation) {
                cairo_rotate(cairo, (double)data->rotation * M_PI / 180.0);
            }
            if (data->rotation == 90 || data->rotation == 270) {
                // rescale to match landscape viewbox size
                cairo_scale(cairo, (double)pm->height / pm->width,
                            (double)pm->height / pm->width);
            }
            if (data->flip_horizontal) {
                cairo_scale(cairo, -1.0, 1.0);
            }
            if (data->flip_vertical) {
                cairo_scale(cairo, 1.0, -1.0);
            }
            cairo_translate(cairo, -viewbox.width / 2 - x,
                            -viewbox.height / 2 - y);

            rsvg_handle_render_document(data->rsvg_handle, cairo, &viewbox,
                                        NULL);
        }
        cairo_destroy(cairo);
    }
    cairo_surface_destroy(surface);
}

/** Free SVG renderer, see `struct image::decoder::free`. */
static void svg_free(struct imgdata* img)
{
    struct svg_data* data = img->decoder.data;

    if (data->rsvg_handle) {
        g_object_unref(data->rsvg_handle);
    }

    free(img->decoder.data);
}

// SVG loader implementation
enum image_status decode_svg(struct imgdata* img, const uint8_t* data,
                             size_t size)
{
    RsvgHandle* svg;
    RsvgRectangle viewbox;
    gboolean viewbox_valid;
    size_t width, height;
    struct pixmap* pm;

    if (!is_svg(data, size)) {
        return imgload_unsupported;
    }

    svg = rsvg_handle_new_from_data(data, size, NULL);
    if (!svg) {
        return imgload_fmterror;
    }

    // create virtual image pixmap
    rsvg_handle_get_intrinsic_dimensions(svg, NULL, NULL, NULL, NULL,
                                         &viewbox_valid, &viewbox);
    width = VIRTUAL_SIZE_PX;
    height = VIRTUAL_SIZE_PX;
    if (viewbox_valid) {
        if (viewbox.width < viewbox.height) {
            width *= (double)viewbox.width / viewbox.height;
        } else {
            height *= (double)viewbox.height / viewbox.width;
        }
    }
    pm = image_alloc_frame(img, width, height);
    if (!pm) {
        g_object_unref(svg);
        return imgload_fmterror;
    }

    image_set_format(img, "SVG");
    if (viewbox_valid) {
        image_add_info(img, "Real size", "%0.2fx%0.2f", viewbox.width,
                       viewbox.height);
    }
    img->alpha = true;

    // use custom renderer
    img->decoder.data = calloc(1, sizeof(struct svg_data));
    if (!img->decoder.data) {
        g_object_unref(svg);
        return imgload_unknown;
    }

    img->decoder.render = svg_render;
    img->decoder.free = svg_free;
    img->decoder.flip = svg_flip;
    img->decoder.rotate = svg_rotate;
    ((struct svg_data*)img->decoder.data)->rsvg_handle = svg;

    // render to virtual pixmap to use it in the export action
    svg_render(img, 1.0, 0, 0, pm);

    return imgload_success;
}
