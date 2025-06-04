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

// Canvas size
#define CANVAS_SIZE_MIN     500
#define CANVAS_SIZE_MAX     2000
#define CANVAS_SIZE_DEFAULT 1000

// Max offset of the root svg node in xml file
#define MAX_SIGNATURE_OFFSET 1024

/** SVG specific decoder data */
struct svg_data {
    RsvgHandle* rsvg;     ///< RSVG handle containing the image data
    double offset_x;      ///< Horizontal offset relative to canvas
    double offset_y;      ///< Vertical offset relative to canvas
    size_t rotation;      ///< Rotation in degrees
    bool flip_vertical;   ///< Whether to flip the image vertically
    bool flip_horizontal; ///< Whether to flip the image horizontally
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
    if (max_pos > MAX_SIGNATURE_OFFSET) {
        max_pos = MAX_SIGNATURE_OFFSET;
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
 * Rotate the SVG (update internal rotation variable).
 * @param img image container
 * @param angle rotation angle (only 90, 180, or 270)
 */
static void svg_rotate(struct imgdata* img, size_t angle)
{
    struct svg_data* svg = img->decoder.data;
    svg->rotation += angle;
    svg->rotation %= 360;
}

/**
 * Flip the SVG (set internal flip flag).
 * @param img image container
 * @param vertical true for vertical flip, false for horizontal
 */
static void svg_flip(struct imgdata* img, bool vertical)
{
    struct svg_data* svg = img->decoder.data;
    if (vertical) {
        svg->flip_vertical = !svg->flip_vertical;
    } else {
        svg->flip_horizontal = !svg->flip_horizontal;
    }
}

/** Custom SVG renderer, see `struct image::decoder::renderer`. */
static void svg_render(struct imgdata* img, double scale, ssize_t x, ssize_t y,
                       struct pixmap* dst)
{
    cairo_t* cairo = NULL;
    cairo_surface_t* surface = NULL;
    const struct pixmap* pm = &((struct imgframe*)arr_nth(img->frames, 0))->pm;
    struct svg_data* svg = img->decoder.data;
    const RsvgRectangle viewbox = {
        .x = x + scale * svg->offset_x,
        .y = y - scale * svg->offset_y,
        .width = scale * pm->width,
        .height = scale * pm->height,
    };

    // prepare cairo surface
    surface = cairo_image_surface_create_for_data(
        (uint8_t*)dst->data, CAIRO_FORMAT_ARGB32, dst->width, dst->height,
        dst->width * sizeof(argb_t));
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        goto done;
    }
    cairo = cairo_create(surface);
    if (cairo_status(cairo) != CAIRO_STATUS_SUCCESS) {
        goto done;
    }

    // render svg to cairo surface
    cairo_translate(cairo, viewbox.width / 2 + x, viewbox.height / 2 + y);
    if (svg->rotation) {
        cairo_rotate(cairo, (double)svg->rotation * M_PI / 180.0);
        if (svg->rotation == 90 || svg->rotation == 270) {
            // rescale to match landscape viewbox size
            const double scale = (double)pm->height / pm->width;
            cairo_scale(cairo, scale, scale);
        }
    }
    if (svg->flip_horizontal) {
        cairo_scale(cairo, -1.0, 1.0);
    }
    if (svg->flip_vertical) {
        cairo_scale(cairo, 1.0, -1.0);
    }
    cairo_translate(cairo, -viewbox.width / 2 - x, -viewbox.height / 2 - y);

    rsvg_handle_render_document(svg->rsvg, cairo, &viewbox, NULL);

done:
    if (cairo) {
        cairo_destroy(cairo);
    }
    if (surface) {
        cairo_surface_destroy(surface);
    }
}

/** Free SVG renderer, see `struct image::decoder::free`. */
static void svg_free(struct imgdata* img)
{
    g_object_unref(((struct svg_data*)img->decoder.data)->rsvg);
    free(img->decoder.data);
}

/**
 * Get canvas size.
 * @param rsvg SVG image handle
 * @param canvas output canvas rectange
 */
static void get_canvas(RsvgHandle* rsvg, RsvgRectangle* canvas)
{
    RsvgLength svg_w, svg_h;
    RsvgRectangle viewbox = { 0 };
    gboolean width_ok = TRUE, height_ok = TRUE, viewbox_ok = TRUE;

    rsvg_handle_get_intrinsic_dimensions(rsvg, &width_ok, &svg_w, &height_ok,
                                         &svg_h, &viewbox_ok, &viewbox);
    if (viewbox_ok) {
        *canvas = viewbox;
    } else if (width_ok && height_ok) {
        canvas->width = svg_w.length;
        canvas->height = svg_h.length;
        if (svg_w.unit == RSVG_UNIT_PERCENT) {
            canvas->width *= CANVAS_SIZE_DEFAULT;
            canvas->height *= CANVAS_SIZE_DEFAULT;
        }
    } else {
        canvas->width = CANVAS_SIZE_DEFAULT;
        canvas->height = CANVAS_SIZE_DEFAULT;
    }

    if (canvas->width < CANVAS_SIZE_MIN || canvas->height < CANVAS_SIZE_MIN) {
        const double scale = (double)CANVAS_SIZE_MIN /
            (canvas->width > canvas->height ? canvas->width : canvas->height);
        canvas->width *= scale;
        canvas->height *= scale;
    }
    if (canvas->width > CANVAS_SIZE_MAX || canvas->height > CANVAS_SIZE_MAX) {
        const double scale = (double)CANVAS_SIZE_MAX /
            (canvas->width > canvas->height ? canvas->width : canvas->height);
        canvas->width *= scale;
        canvas->height *= scale;
    }
}

/**
 * Get description of real image size.
 * @param rsvg SVG image handle
 * @param width output buffer to store width
 * @param max_w size of the `width` buffer
 * @param height output buffer to store height
 * @param max_h size of the `height` buffer
 */
static void get_real_size(RsvgHandle* rsvg, char* width, size_t max_w,
                          char* height, size_t max_h)
{
    RsvgLength svg_w, svg_h;
    RsvgRectangle viewbox;
    gboolean width_ok = TRUE, height_ok = TRUE, viewbox_ok = TRUE;
    const char* units;

    rsvg_handle_get_intrinsic_dimensions(rsvg, &width_ok, &svg_w, &height_ok,
                                         &svg_h, &viewbox_ok, &viewbox);

    if (width_ok && height_ok && svg_w.length != 1.0 && svg_h.length != 1.0) {
        switch (svg_w.unit) {
            case RSVG_UNIT_PERCENT:
                svg_w.length *= 100;
                svg_h.length *= 100;
                units = "%";
                break;
            case RSVG_UNIT_PX:
                units = "px";
                break;
            case RSVG_UNIT_EM:
                units = "em";
                break;
            case RSVG_UNIT_EX:
                units = "ex";
                break;
            case RSVG_UNIT_IN:
                units = "in";
                break;
            case RSVG_UNIT_CM:
                units = "cm";
                break;
            case RSVG_UNIT_MM:
                units = "mm";
                break;
            case RSVG_UNIT_PT:
                units = "pt";
                break;
            case RSVG_UNIT_PC:
                units = "pc";
                break;
            case RSVG_UNIT_CH:
                units = "ch";
                break;
            default:
                units = "";
                break;
        }
    } else if (viewbox_ok) {
        svg_w.length = viewbox.width;
        svg_h.length = viewbox.height;
        units = "px";
    } else {
        svg_w.length = 100;
        svg_h.length = 100;
        units = "%";
    }

    snprintf(width, max_w, "%0.2f%s", svg_w.length, units);
    snprintf(height, max_h, "%0.2f%s", svg_h.length, units);
}

// SVG loader implementation
enum image_status decode_svg(struct imgdata* img, const uint8_t* data,
                             size_t size)
{
    struct imgdec* decoder = &img->decoder;
    char real_width[64] = { 0 };
    char real_height[64] = { 0 };
    RsvgRectangle canvas = { 0 };
    struct svg_data* svg;
    struct pixmap* pm;
    RsvgHandle* rsvg;

    if (!is_svg(data, size)) {
        return imgload_unsupported;
    }

    rsvg = rsvg_handle_new_from_data(data, size, NULL);
    if (!rsvg) {
        return imgload_fmterror;
    }

    // create decoder context for custom rendering
    svg = calloc(1, sizeof(*svg));
    if (!svg) {
        g_object_unref(rsvg);
        return imgload_fmterror;
    }
    svg->rsvg = rsvg;
    decoder->data = svg;
    decoder->free = svg_free;
    decoder->flip = svg_flip;
    decoder->rotate = svg_rotate;
    decoder->render = svg_render;

    // get canvas size and offset
    get_canvas(rsvg, &canvas);
    if (canvas.x) {
        svg->offset_x = canvas.width / canvas.x;
    }
    if (canvas.y) {
        svg->offset_y = canvas.height / canvas.y;
    }

    // render to pixmap that will be used in the export action
    pm = image_alloc_frame(img, pixmap_argb, canvas.width, canvas.height);
    if (!pm) {
        return imgload_fmterror;
    }
    svg_render(img, 1.0, 0, 0, pm);

    // set image properties
    image_set_format(img, "SVG");
    get_real_size(rsvg, real_width, sizeof(real_width) - 1, real_height,
                  sizeof(real_height) - 1);
    image_add_info(img, "SVG size", "%s x %s", real_width, real_height);

    return imgload_success;
}
