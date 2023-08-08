// SPDX-License-Identifier: MIT
// Canvas used to render images and text to window buffer.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "canvas.h"

#include "font.h"

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
    size_t wnd_scale;      ///< Window scale factor (HiDPI)
    struct font* font;     ///< Font context
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
    if (img.width <= ctx->window.width) {
        ctx->image.x = ctx->window.width / 2 - img.width / 2;
    }
    if (img.height <= ctx->window.height) {
        ctx->image.y = ctx->window.height / 2 - img.height / 2;
    }
}

/**
 * Get average color of adjustment pixels.
 * @param pixmap source array of pixels
 * @param stride number of pixels per line in the array
 * @param vp vieweport to restrict depth
 * @param x,y central pixel coordinates
 * @return average color
 */
static inline argb_t get_average(const argb_t* pixmap, size_t stride,
                                 const struct rect* vp, ssize_t x, ssize_t y)
{
    argb_t a, r, g, b;
    argb_t pixel;
    size_t count;

    pixel = pixmap[y * stride + x];
    a = ARGB_GET_A(pixel);
    r = ARGB_GET_R(pixel);
    g = ARGB_GET_G(pixel);
    b = ARGB_GET_B(pixel);
    count = 1;

    if (x > vp->x) {
        pixel = pixmap[y * stride + x - 1];
        r += ARGB_GET_R(pixel);
        g += ARGB_GET_G(pixel);
        b += ARGB_GET_B(pixel);
        ++count;
    }
    if (x + 1 < vp->x + (ssize_t)vp->width) {
        pixel = pixmap[y * stride + x + 1];
        r += ARGB_GET_R(pixel);
        g += ARGB_GET_G(pixel);
        b += ARGB_GET_B(pixel);
        ++count;
    }
    if (y > vp->y) {
        pixel = pixmap[(y - 1) * stride + x];
        r += ARGB_GET_R(pixel);
        g += ARGB_GET_G(pixel);
        b += ARGB_GET_B(pixel);
        ++count;
    }
    if (y + 1 < vp->y + (ssize_t)vp->height) {
        pixel = pixmap[(y + 1) * stride + x];
        r += ARGB_GET_R(pixel);
        g += ARGB_GET_G(pixel);
        b += ARGB_GET_B(pixel);
        ++count;
    }

    r = (float)r / count;
    g = (float)g / count;
    b = (float)b / count;

    return (ARGB_SET_A(a) | ARGB_SET_R(r) | ARGB_SET_G(g) | ARGB_SET_B(b));
}

struct canvas* canvas_init(struct config* cfg)
{
    struct canvas* ctx;

    ctx = calloc(1, sizeof(*ctx));
    if (ctx) {
        ctx->config = cfg;
        ctx->font = font_init(cfg);
    }

    return ctx;
}

void canvas_free(struct canvas* ctx)
{
    if (ctx) {
        font_free(ctx->font);
        free(ctx);
    }
}

bool canvas_resize_window(struct canvas* ctx, size_t width, size_t height,
                          size_t scale)
{
    const bool first = (ctx->window.width == 0);

    ctx->window.width = width;
    ctx->window.height = height;

    ctx->wnd_scale = scale;
    font_scale(ctx->font, scale);

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
    if (ctx->config->window == COLOR_TRANSPARENT) {
        memset(wnd, 0, ctx->window.width * ctx->window.height * sizeof(argb_t));
    } else {
        for (size_t y = 0; y < ctx->window.height; ++y) {
            argb_t* line = &wnd[y * ctx->window.width];
            for (size_t x = 0; x < ctx->window.width; ++x) {
                line[x] = ARGB_SET_A(0xff) | ctx->config->window;
            }
        }
    }
}

void canvas_draw_image(const struct canvas* ctx, bool alpha, const argb_t* img,
                       argb_t* wnd)
{
    struct rect vp;
    const struct rect ivp = { 0, 0, ctx->image.width, ctx->image.height };

    if (!get_intersection(ctx, &vp)) {
        return; // possible bug: image out of window
    }

    for (size_t y = 0; y < vp.height; ++y) {
        argb_t* wnd_line = &wnd[(vp.y + y) * ctx->window.width + vp.x];
        const size_t img_y = (float)(y + vp.y - ctx->image.y) / ctx->scale;
        for (size_t x = 0; x < vp.width; ++x) {
            const size_t img_x = (float)(x + vp.x - ctx->image.x) / ctx->scale;
            argb_t fg;

            if (ctx->config->antialiasing && ctx->scale >= 1) {
                // anti-aliasing for scale >= 100%
                fg = get_average(img, ctx->image.width, &ivp, img_x, img_y);
            } else {
                fg = img[img_y * ctx->image.width + img_x];
            }

            if (!alpha) {
                wnd_line[x] = fg;
            } else {
                // alpha blending
                const uint8_t alpha = ARGB_GET_A(fg);
                uint8_t alpha_set;
                argb_t bg;

                if (ctx->config->background == COLOR_TRANSPARENT) {
                    bg = 0;
                    alpha_set = alpha;
                } else if (ctx->config->background == BACKGROUND_GRID) {
                    const bool shift = (y / (GRID_STEP * ctx->wnd_scale)) % 2;
                    const size_t tail = x / (GRID_STEP * ctx->wnd_scale);
                    const argb_t grid =
                        (tail % 2) ^ shift ? GRID_COLOR1 : GRID_COLOR2;
                    bg = grid;
                    alpha_set = 0xff;
                } else {
                    bg = ctx->config->background;
                    alpha_set = 0xff;
                }

                wnd_line[x] = ARGB_ALPHA_BLEND(alpha, alpha_set, bg, fg);
            }
        }
    }

    // anti-aliasing for scale < 100%
    if (ctx->config->antialiasing && ctx->scale < 1) {
        for (ssize_t y = vp.y; y < vp.y + (ssize_t)vp.height; ++y) {
            for (ssize_t x = vp.x; x < vp.x + (ssize_t)vp.width; ++x) {
                wnd[y * ctx->window.width + x] =
                    get_average(wnd, ctx->window.width, &vp, x, y);
            }
        }
    }
}

void canvas_print_line(const struct canvas* ctx, argb_t* wnd,
                       enum canvas_corner corner, const char* text)
{
    struct point pos = { 0, 0 };

    switch (corner) {
        case cc_top_right:
            pos.x = ctx->window.width - font_text_width(ctx->font, text, 0) -
                TEXT_PADDING;
            pos.y = TEXT_PADDING;
            break;
        case cc_bottom_left:
            pos.x = TEXT_PADDING;
            pos.y = ctx->window.height - font_height(ctx->font) - TEXT_PADDING;
            break;
        case cc_bottom_right:
            pos.x = ctx->window.width - font_text_width(ctx->font, text, 0) -
                TEXT_PADDING;
            pos.y = ctx->window.height - font_height(ctx->font) - TEXT_PADDING;
            break;
    }

    font_print(ctx->font, wnd, &ctx->window, &pos, text, 0);
}

void canvas_print_info(const struct canvas* ctx, argb_t* wnd, size_t num,
                       const struct info_table* info)
{
    size_t val_offset = 0;
    struct point pos = { .x = TEXT_PADDING, .y = TEXT_PADDING };

    // draw keys block
    for (size_t i = 0; i < num; ++i) {
        const char* key = info[i].key;
        if (key && *key) {
            size_t width;
            struct point pos_delim;
            width = font_print(ctx->font, wnd, &ctx->window, &pos, key, 0);
            pos_delim.x = TEXT_PADDING + width;
            pos_delim.y = pos.y;
            width +=
                font_print(ctx->font, wnd, &ctx->window, &pos_delim, ":", 0) *
                3;
            if (width > val_offset) {
                val_offset = width;
            }
        }
        pos.y += font_height(ctx->font);
    }

    // draw values block
    pos.x = TEXT_PADDING + val_offset + TEXT_PADDING;
    pos.y = TEXT_PADDING;
    for (size_t i = 0; i < num; ++i) {
        const char* value = info[i].value;
        if (value && *value) {
            font_print(ctx->font, wnd, &ctx->window, &pos, value, 0);
        }
        pos.y += font_height(ctx->font);
    }
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
    if (sc == cs_fit_or100 || sc == cs_fit_window || sc == cs_fill_window) {
        const float sw = 1.0 / ((float)ctx->image.width / ctx->window.width);
        const float sh = 1.0 / ((float)ctx->image.height / ctx->window.height);
        if (sc == cs_fill_window) {
            ctx->scale = max(sw, sh);
        } else {
            ctx->scale = min(sw, sh);
            if (sc == cs_fit_or100 && ctx->scale > 1.0) {
                ctx->scale = 1.0;
            }
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
