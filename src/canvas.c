// SPDX-License-Identifier: MIT
// Canvas used to render images and text to window buffer.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "canvas.h"

#include <cairo/cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Background grid parameters
#define GRID_STEP   10
#define GRID_COLOR1 0xff333333
#define GRID_COLOR2 0xff4c4c4c

// Scale thresholds
#define MIN_SCALE 10    // pixels
#define MAX_SCALE 100.0 // factor

/** Text padding: space between text layout and window edge. */
#define TEXT_PADDING 10

#define max(x, y) ((x) > (y) ? (x) : (y))
#define min(x, y) ((x) < (y) ? (x) : (y))

/** Canvas context. */
struct canvas {
    struct config* config; ///< Configuration
    float scale;           ///< Scale, 1.0 = 100%
    struct rect image;     ///< Image position and size
    struct size window;    ///< Output window size
    void* font_handle;     ///< Font handle to draw text
};

/**
 * Get intersection between window and image.
 * @param ctx canvas context
 * @param rect intersection coordinates and size
 * @return false if window and image don't intersect
 */
static bool get_intersection(const struct canvas* ctx, struct rect* rect)
{
    const ssize_t scaled_x = ctx->image.x + ctx->scale * ctx->image.width;
    const ssize_t scaled_y = ctx->image.y + ctx->scale * ctx->image.height;
    const ssize_t pos_left = max(0, ctx->image.x);
    const ssize_t pos_top = max(0, ctx->image.y);
    const ssize_t pos_right = min((ssize_t)ctx->window.width, scaled_x);
    const ssize_t pos_bottom = min((ssize_t)ctx->window.height, scaled_y);

    if (pos_left < pos_right && pos_top < pos_bottom) {
        rect->x = pos_left;
        rect->y = pos_top;
        rect->width = pos_right - pos_left;
        rect->height = pos_bottom - pos_top;
        return true;
    }

    return false;
}

/**
 * Fix viewport position to minimize gap between image and window edge.
 * @param ctx canvas context
 */
static void fix_viewport(struct canvas* ctx)
{
    const struct rect img = { .x = ctx->image.x,
                              .y = ctx->image.y,
                              .width = ctx->scale * ctx->image.width,
                              .height = ctx->scale * ctx->image.height };

    if (img.width <= ctx->window.width) {
        ctx->image.x = ctx->window.width / 2 - img.width / 2;
    }
    if (img.height <= ctx->window.height) {
        ctx->image.y = ctx->window.height / 2 - img.height / 2;
    }
    if (img.x > 0 && img.x + img.width > ctx->window.width) {
        ctx->image.x = 0;
    }
    if (img.y > 0 && img.y + img.height > ctx->window.height) {
        ctx->image.y = 0;
    }
    if (img.x < 0 && img.x + img.width < ctx->window.width) {
        ctx->image.x = ctx->window.width - img.width;
    }
    if (img.y < 0 && img.y + img.height < ctx->window.height) {
        ctx->image.y = ctx->window.height - img.height;
    }
}

struct canvas* canvas_init(struct config* cfg)
{
    struct canvas* ctx;

    ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }

    ctx->config = cfg;
    ctx->font_handle = pango_font_description_from_string(cfg->font_face);

    return ctx;
}

void canvas_free(struct canvas* ctx)
{
    if (ctx) {
        if (ctx->font_handle) {
            pango_font_description_free(ctx->font_handle);
        }
        free(ctx);
    }
}

bool canvas_resize_window(struct canvas* ctx, size_t width, size_t height)
{
    const bool first = (ctx->window.width == 0);

    ctx->window.width = width;
    ctx->window.height = height;
    fix_viewport(ctx);

    return first;
}

void canvas_reset_image(struct canvas* ctx, size_t width, size_t height,
                        enum canvas_scale sc)
{
    ctx->image.x = 0;
    ctx->image.y = 0;
    ctx->image.width = width;
    ctx->image.height = height;
    ctx->scale = 0;
    canvas_set_scale(ctx, sc);
}

void canvas_swap_image_size(struct canvas* ctx)
{
    const ssize_t diff = (ssize_t)ctx->image.width - ctx->image.height;
    const ssize_t shift = (ctx->scale * diff) / 2;
    const size_t old_width = ctx->image.width;

    ctx->image.x += shift;
    ctx->image.y -= shift;
    ctx->image.width = ctx->image.height;
    ctx->image.height = old_width;

    fix_viewport(ctx);
}

void canvas_clear(const struct canvas* ctx, argb_t* wnd)
{
    if (ctx->config->frame == COLOR_TRANSPARENT) {
        memset(wnd, 0, ctx->window.width * ctx->window.height * sizeof(argb_t));
    } else {
        for (size_t y = 0; y < ctx->window.height; ++y) {
            argb_t* line = &wnd[y * ctx->window.width];
            for (size_t x = 0; x < ctx->window.width; ++x) {
                line[x] = ARGB_ALPHA_MASK | ctx->config->frame;
            }
        }
    }
}

void canvas_draw_image(const struct canvas* ctx, bool alpha, const argb_t* img,
                       argb_t* wnd)
{
    struct rect vp;

    if (!get_intersection(ctx, &vp)) {
        return; // possible bug: image out of window
    }

    for (size_t y = 0; y < vp.height; ++y) {
        // start offset of window buffer
        argb_t* wnd_line = &wnd[(vp.y + y) * ctx->window.width + vp.x];
        // start offset of image buffer
        const size_t img_y = (float)(y + vp.y - ctx->image.y) / ctx->scale;
        const size_t img_x = (float)(vp.x - ctx->image.x) / ctx->scale;
        const argb_t* img_line = &img[img_y * ctx->image.width + img_x];

        // fill window buffer
        for (size_t x = 0; x < vp.width; ++x) {
            const argb_t fg = img_line[(size_t)((float)x / ctx->scale)];

            if (!alpha) {
                wnd_line[x] = fg;
            } else {
                // alpha blend
                const uint8_t alpha = fg >> ARGB_ALPHA_SHIFT;
                const uint16_t alpha_inv = 256 - alpha;
                argb_t alpha_set, bg;

                if (ctx->config->background == COLOR_TRANSPARENT) {
                    bg = 0;
                    alpha_set = fg & ARGB_ALPHA_MASK;
                } else if (ctx->config->background == BACKGROUND_GRID) {
                    const bool shift = (y / GRID_STEP) % 2;
                    const size_t tail = x / GRID_STEP;
                    const argb_t grid =
                        (tail % 2) ^ shift ? GRID_COLOR1 : GRID_COLOR2;
                    bg = grid;
                    alpha_set = ARGB_ALPHA_MASK;
                } else {
                    bg = ctx->config->background;
                    alpha_set = ARGB_ALPHA_MASK;
                }
                // clang-format off
                wnd_line[x] = alpha_set |
                    ((alpha * ((fg >> 16) & 0xff) + alpha_inv * ((bg >> 16) & 0xff)) >> 8) << 16 |
                    ((alpha * ((fg >>  8) & 0xff) + alpha_inv * ((bg >>  8) & 0xff)) >> 8) << 8 |
                    ((alpha * ((fg >>  0) & 0xff) + alpha_inv * ((bg >>  0) & 0xff)) >> 8);
                // clang-format on
            }
        }
    }
}

void canvas_print(const struct canvas* ctx, argb_t* wnd,
                  enum canvas_corner corner, const char* text)
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
        (uint8_t*)wnd, CAIRO_FORMAT_ARGB32, ctx->window.width,
        ctx->window.height, ctx->window.width * sizeof(argb_t));
    cairo = cairo_create(window);
    layout = pango_cairo_create_layout(cairo);
    if (!layout) {
        return;
    }

    // setup layout
    pango_layout_set_font_description(layout, ctx->font_handle);
    pango_layout_set_text(layout, text, -1);

    // magic, need to handle tabs in a right way
    tab = pango_tab_array_new_with_positions(1, true, PANGO_TAB_LEFT, 150);
    pango_layout_set_tabs(layout, tab);
    pango_tab_array_free(tab);

    // calculate layout position
    pango_layout_get_size(layout, &txt_w, &txt_h);
    txt_w /= PANGO_SCALE;
    txt_h /= PANGO_SCALE;
    switch (corner) {
        case cc_top_left:
            x = TEXT_PADDING;
            y = TEXT_PADDING;
            break;
        case cc_top_right:
            x = ctx->window.width - txt_w - TEXT_PADDING;
            y = TEXT_PADDING;
            break;
        case cc_bottom_left:
            x = TEXT_PADDING;
            y = ctx->window.height - txt_h - TEXT_PADDING;
            break;
        case cc_bottom_right:
            x = ctx->window.width - txt_w - TEXT_PADDING;
            y = ctx->window.height - txt_h - TEXT_PADDING;
            break;
    }

    // put layout on cairo surface
    cairo_set_source_rgb(
        cairo, ((double)((ctx->config->font_color >> 16) & 0xff) / 255),
        ((double)((ctx->config->font_color >> 8) & 0xff) / 255),
        ((double)(ctx->config->font_color & 0xff) / 255));
    cairo_move_to(cairo, x, y);
    pango_cairo_show_layout(cairo, layout);

    g_object_unref(layout);
    cairo_destroy(cairo);
    cairo_surface_destroy(window);
}

bool canvas_move(struct canvas* ctx, enum canvas_move mv)
{
    const size_t scaled_width = ctx->scale * ctx->image.width;
    const size_t scaled_height = ctx->scale * ctx->image.height;
    const size_t step_x = ctx->window.width / 10;
    const size_t step_y = ctx->window.height / 10;
    ssize_t prev_x = ctx->image.x;
    ssize_t prev_y = ctx->image.y;

    switch (mv) {
        case cm_center:
            ctx->image.x = ctx->window.width / 2 - scaled_width / 2;
            ctx->image.y = ctx->window.height / 2 - scaled_height / 2;
            break;
        case cm_cnt_hor:
            ctx->image.x = ctx->window.width / 2 - scaled_width / 2;
            break;
        case cm_cnt_vert:
            ctx->image.y = ctx->window.height / 2 - scaled_height / 2;
            break;
        case cm_step_left:
            ctx->image.x += step_x;
            break;
        case cm_step_right:
            ctx->image.x -= step_x;
            break;
        case cm_step_up:
            ctx->image.y += step_y;
            break;
        case cm_step_down:
            ctx->image.y -= step_y;
            break;
    }

    fix_viewport(ctx);

    return (ctx->image.x != prev_x || ctx->image.y != prev_y);
}

void canvas_set_scale(struct canvas* ctx, enum canvas_scale sc)
{
    const float prev = ctx->scale;

    // set new scale factor
    if (sc == cs_fit_or100 || sc == cs_fit_window) {
        const float sw = 1.0 / ((float)ctx->image.width / ctx->window.width);
        const float sh = 1.0 / ((float)ctx->image.height / ctx->window.height);
        ctx->scale = min(sw, sh);
        if (sc == cs_fit_or100 && ctx->scale > 1.0) {
            ctx->scale = 1.0;
        }
    } else if (sc == cs_real_size) {
        ctx->scale = 1.0; // 100 %
    } else {
        const float step = ctx->scale / 10.0;
        if (sc == cs_zoom_in) {
            ctx->scale += step;
            if (ctx->scale > MAX_SCALE) {
                ctx->scale = MAX_SCALE;
            }
        } else if (sc == cs_zoom_out) {
            const float sw = (float)MIN_SCALE / ctx->image.width;
            const float sh = (float)MIN_SCALE / ctx->image.height;
            const float scale_min = max(sw, sh);
            ctx->scale -= step;
            if (ctx->scale < scale_min) {
                ctx->scale = scale_min;
            }
        }
    }

    // move viewport
    if (sc != cs_zoom_in && sc != cs_zoom_out) {
        canvas_move(ctx, cm_center);
    } else {
        // move to save the center of previous coordinates
        const size_t old_w = prev * ctx->image.width;
        const size_t old_h = prev * ctx->image.height;
        const size_t new_w = ctx->scale * ctx->image.width;
        const size_t new_h = ctx->scale * ctx->image.height;
        const ssize_t delta_w = old_w - new_w;
        const ssize_t delta_h = old_h - new_h;
        const ssize_t cntr_x = ctx->window.width / 2 - ctx->image.x;
        const ssize_t cntr_y = ctx->window.height / 2 - ctx->image.y;
        ctx->image.x += ((float)cntr_x / old_w) * delta_w;
        ctx->image.y += ((float)cntr_y / old_h) * delta_h;
        fix_viewport(ctx);
    }
}

float canvas_get_scale(struct canvas* ctx)
{
    return ctx->scale;
}
