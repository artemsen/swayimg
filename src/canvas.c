// SPDX-License-Identifier: MIT
// Canvas used to render images and text to window buffer.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "canvas.h"

#include "window.h"

#include <cairo/cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Background grid parameters
#define GRID_STEP   10
#define GRID_COLOR1 0x333333
#define GRID_COLOR2 0x4c4c4c

// Scale thresholds
#define MIN_SCALE 10    // pixels
#define MAX_SCALE 100.0 // factor

/** Text padding: space between text layout and window edge. */
#define TEXT_PADDING 10

#define ROTATE_RAD(r) ((r * 90) * 3.14159 / 180)

void init_canvas(struct canvas* ctx, size_t width, size_t height, config_t* cfg)
{
    ctx->wnd_w = width;
    ctx->wnd_h = height;

    // initialize font
    ctx->font_handle = pango_font_description_from_string(cfg->font_face);
    ctx->font_color = cfg->font_color;
}

void free_canvas(struct canvas* ctx)
{
    if (ctx->font_handle) {
        pango_font_description_free(ctx->font_handle);
    }
}

void attach_image(struct canvas* ctx, const image_t* img)
{
    ctx->img_x = 0;
    ctx->img_y = 0;
    ctx->img_w = img->width;
    ctx->img_h = img->height;

    ctx->scale = 0.0;
    ctx->rotate = rotate_0;
    ctx->flip = flip_none;
}

void attach_window(struct canvas* ctx, struct window* wnd, uint32_t color)
{
    ctx->wnd_w = wnd->width;
    ctx->wnd_h = wnd->height;
    for (size_t y = 0; y < wnd->height; ++y) {
        uint32_t* line = &wnd->data[y * wnd->width];
        for (size_t x = 0; x < wnd->width; ++x) {
            line[x] = color;
        }
    }
}

void draw_image(const struct canvas* ctx, const image_t* img,
                struct window* wnd)
{
    const ssize_t center_x = ctx->img_w / 2;
    const ssize_t center_y = ctx->img_h / 2;
    cairo_surface_t *window, *image;
    cairo_t* cairo;
    cairo_matrix_t matrix;

    window = cairo_image_surface_create_for_data(
        (uint8_t*)wnd->data, CAIRO_FORMAT_ARGB32, wnd->width, wnd->height,
        wnd->width * sizeof(wnd->data[0]));
    image = cairo_image_surface_create_for_data(
        (uint8_t*)img->data, CAIRO_FORMAT_ARGB32, img->width, img->height,
        img->width * sizeof(img->data[0]));

    cairo = cairo_create(window);
    cairo_get_matrix(cairo, &matrix);
    cairo_matrix_translate(&matrix, ctx->img_x, ctx->img_y);

    // apply scale
    cairo_matrix_scale(&matrix, ctx->scale, ctx->scale);

    // apply flip
    if (ctx->flip) {
        cairo_matrix_translate(&matrix, center_x, center_y);
        if (ctx->flip & flip_vertical) {
            matrix.yy = -matrix.yy;
        }
        if (ctx->flip & flip_horizontal) {
            matrix.xx = -matrix.xx;
        }
        cairo_matrix_translate(&matrix, -center_x, -center_y);
    }

    // apply rotate
    if (ctx->rotate) {
        cairo_matrix_translate(&matrix, center_x, center_y);
        cairo_matrix_rotate(&matrix, ROTATE_RAD(ctx->rotate));
        cairo_matrix_translate(&matrix, -center_x, -center_y);
    }

    // draw
    cairo_set_matrix(cairo, &matrix);
    cairo_set_source_surface(cairo, image, 0, 0);
    cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
    cairo_paint(cairo);

    cairo_destroy(cairo);
    cairo_surface_destroy(image);
    cairo_surface_destroy(window);
}

static inline ssize_t min(ssize_t a, ssize_t b)
{
    return a < b ? a : b;
}
static inline ssize_t max(ssize_t a, ssize_t b)
{
    return a > b ? a : b;
}
//#define max(x, y) (((x) > (y)) ? (x) : (y))
//#define min(x, y) (((x) < (y)) ? (x) : (y))

void draw_grid(const struct canvas* ctx, struct window* wnd)
{
    const ssize_t img_x = ctx->img_x + ctx->scale * ctx->img_w;
    const ssize_t img_y = ctx->img_y + ctx->scale * ctx->img_h;
    const ssize_t gtl_x = max(0, ctx->img_x);
    const ssize_t gtl_y = max(0, ctx->img_y);
    const ssize_t gbr_x = min((ssize_t)wnd->width, img_x);
    const ssize_t gbr_y = min((ssize_t)wnd->height, img_y);

    size_t width, stride;

    if (gtl_x > gbr_x || gtl_y > gbr_y) {
        return; // possible bug: image out of window
    }

    width = gbr_x - gtl_x;
    stride = width * sizeof(wnd->data[0]);
    for (ssize_t y = gtl_y; y < gbr_y; ++y) {
        uint32_t* line = &wnd->data[y * wnd->width + gtl_x];
        const size_t grid_line = y - gtl_y;
        if (grid_line > GRID_STEP * 2) {
            const uint32_t* pattern = line - (wnd->width * (GRID_STEP * 2));
            memcpy(line, pattern, stride);
        } else {
            // fill template
            const bool shift = (grid_line / GRID_STEP) % 2;
            for (size_t x = 0; x < width; ++x) {
                const size_t tail = x / GRID_STEP;
                line[x] = (tail % 2) ^ shift ? GRID_COLOR1 : GRID_COLOR2;
            }
        }
    }
}

void print_text(const struct canvas* ctx, struct window* wnd,
                text_position_t pos, const char* text)
{
    PangoLayout* layout;
    PangoTabArray* tab;
    cairo_surface_t* window;
    cairo_t* cairo;
    int x = 0, y = 0;
    int txt_w = 0, txt_h = 0;

    if (!ctx->font_handle) {
        return;
    }

    window = cairo_image_surface_create_for_data(
        (uint8_t*)wnd->data, CAIRO_FORMAT_ARGB32, wnd->width, wnd->height,
        wnd->width * sizeof(wnd->data[0]));
    cairo = cairo_create(window);

    layout = pango_cairo_create_layout(cairo);
    if (!layout) {
        return;
    }

    // setup layout
    pango_layout_set_font_description(layout, ctx->font_handle);
    pango_layout_set_text(layout, text, -1);

    // magic, need to handle tabs in a right way
    tab = pango_tab_array_new_with_positions(1, true, PANGO_TAB_LEFT, 100);
    pango_layout_set_tabs(layout, tab);
    pango_tab_array_free(tab);

    // calculate layout position
    pango_layout_get_size(layout, &txt_w, &txt_h);
    txt_w /= PANGO_SCALE;
    txt_h /= PANGO_SCALE;
    switch (pos) {
        case text_top_left:
            x = TEXT_PADDING;
            y = TEXT_PADDING;
            break;
        case text_top_right:
            x = wnd->width - txt_w - TEXT_PADDING;
            y = TEXT_PADDING;
            break;
        case text_bottom_left:
            x = TEXT_PADDING;
            y = wnd->height - txt_h - TEXT_PADDING;
            break;
        case text_bottom_right:
            x = wnd->width - txt_w - TEXT_PADDING;
            y = wnd->height - txt_h - TEXT_PADDING;
            break;
    }

    // put layout on cairo surface
    cairo_set_source_rgb(cairo,
                         ((double)((ctx->font_color >> 16) & 0xff) / 255),
                         ((double)((ctx->font_color >> 8) & 0xff) / 255),
                         ((double)(ctx->font_color & 0xff) / 255));
    cairo_move_to(cairo, x, y);
    pango_cairo_show_layout(cairo, layout);

    g_object_unref(layout);
    cairo_destroy(cairo);
    cairo_surface_destroy(window);
}

bool move_viewpoint(struct canvas* ctx, move_t direction)
{
    const size_t img_w = ctx->scale * ctx->img_w;
    const size_t img_h = ctx->scale * ctx->img_h;
    const size_t step_x = ctx->wnd_w / 10;
    const size_t step_y = ctx->wnd_h / 10;
    ssize_t x = ctx->img_x;
    ssize_t y = ctx->img_y;

    switch (direction) {
        case center_vertical:
            y = ctx->wnd_h / 2 - img_h / 2;
            break;
        case center_horizontal:
            x = ctx->wnd_w / 2 - img_w / 2;
            break;
        case step_left:
            if (x <= 0) {
                x += step_x;
                if (x > 0) {
                    x = 0;
                }
            }
            break;
        case step_right:
            if (x + img_w >= ctx->wnd_w) {
                x -= step_x;
                if (x + img_w < ctx->wnd_w) {
                    x = ctx->wnd_w - img_w;
                }
            }
            break;
        case step_up:
            if (y <= 0) {
                y += step_y;
                if (y > 0) {
                    y = 0;
                }
            }
            break;
        case step_down:
            if (y + img_h >= ctx->wnd_h) {
                y -= step_y;
                if (y + img_h < ctx->wnd_h) {
                    y = ctx->wnd_h - img_h;
                }
            }
            break;
    }

    if (ctx->img_x != x || ctx->img_y != y) {
        ctx->img_x = x;
        ctx->img_y = y;
        return true;
    }

    return false;
}

/**
 * Move view point by scale delta considering window center.
 * @param[in] canvas parameters of the image
 * @param[in] delta scale delta
 */
static void move_scaled(struct canvas* ctx, double delta)
{
    const size_t old_w = (ctx->scale - delta) * ctx->img_w;
    const size_t old_h = (ctx->scale - delta) * ctx->img_h;
    const size_t new_w = ctx->scale * ctx->img_w;
    const size_t new_h = ctx->scale * ctx->img_h;

    if (new_w < ctx->wnd_w) {
        // fits into window width
        move_viewpoint(ctx, center_horizontal);
    } else {
        // move to save the center of previous coordinates
        const int delta_w = old_w - new_w;
        const int cntr_x = ctx->wnd_w / 2 - ctx->img_x;
        const int delta_x = ((double)cntr_x / old_w) * delta_w;
        if (delta_x) {
            ctx->img_x += delta_x;
            if (ctx->img_x > 0) {
                ctx->img_x = 0;
            }
        }
    }

    if (new_h < ctx->wnd_h) {
        //  fits into window height
        move_viewpoint(ctx, center_vertical);
    } else {
        // move to save the center of previous coordinates
        const int delta_h = old_h - new_h;
        const int cntr_y = ctx->wnd_h / 2 - ctx->img_y;
        const int delta_y = ((double)cntr_y / old_h) * delta_h;
        if (delta_y) {
            ctx->img_y += delta_y;
            if (ctx->img_y > 0) {
                ctx->img_y = 0;
            }
        }
    }
}

bool apply_scale(struct canvas* ctx, scale_t op)
{
    const bool swap = (ctx->rotate == rotate_90 || ctx->rotate == rotate_270);
    const double max_w = swap ? ctx->img_h : ctx->img_w;
    const double max_h = swap ? ctx->img_w : ctx->img_h;
    const double step = ctx->scale / 10.0;

    double scale = ctx->scale;

    switch (op) {
        case scale_fit_or100: {
            const double scale_w = 1.0 / (max_w / ctx->wnd_w);
            const double scale_h = 1.0 / (max_h / ctx->wnd_h);
            scale = scale_w < scale_h ? scale_w : scale_h;
            if (scale > 1.0) {
                scale = 1.0;
            }
            break;
        }
        case scale_fit_window: {
            const double scale_w = 1.0 / (max_w / ctx->wnd_w);
            const double scale_h = 1.0 / (max_h / ctx->wnd_h);
            scale = scale_h < scale_w ? scale_h : scale_w;
            break;
        }
        case scale_100:
            scale = 1.0; // 100 %
            break;
        case zoom_in:
            if (ctx->scale < MAX_SCALE) {
                scale = ctx->scale + step;
                if (scale > MAX_SCALE) {
                    scale = MAX_SCALE;
                }
            }
            break;
        case zoom_out:
            scale -= step;
            if (scale * ctx->img_w < MIN_SCALE &&
                scale * ctx->img_h < MIN_SCALE) {
                scale = ctx->scale; // don't change
            }
            break;
    }

    if (ctx->scale != scale) {
        // move viewpoint
        const double delta = scale - ctx->scale;
        ctx->scale = scale;
        if (op == scale_fit_window || op == scale_fit_or100 ||
            op == scale_100) {
            move_viewpoint(ctx, center_vertical);
            move_viewpoint(ctx, center_horizontal);
        } else {
            move_scaled(ctx, delta);
        }
        return true;
    }

    return false;
}

void apply_rotate(struct canvas* ctx, bool clockwise)
{
    if (clockwise) {
        if (ctx->rotate == rotate_270) {
            ctx->rotate = rotate_0;
        } else {
            ++ctx->rotate;
        }
    } else {
        if (ctx->rotate == rotate_0) {
            ctx->rotate = rotate_270;
        } else {
            --ctx->rotate;
        }
    }
}

void apply_flip(struct canvas* ctx, flip_t flip)
{
    ctx->flip ^= flip;
}
