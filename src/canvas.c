// SPDX-License-Identifier: MIT
// Canvas used to render images and text to window buffer.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "canvas.h"

#include "config.h"
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
    float scale;        ///< Scale, 1.0 = 100%
    struct rect image;  ///< Image position and size
    struct size window; ///< Output window size
    size_t wnd_scale;   ///< Window scale factor (HiDPI)
};
static struct canvas ctx;

/**
 * Fix viewport position to minimize gap between image and window edge.
 */
static void fix_viewport(void)
{
    const struct rect img = { .x = ctx.image.x,
                              .y = ctx.image.y,
                              .width = ctx.scale * ctx.image.width,
                              .height = ctx.scale * ctx.image.height };

    if (img.x > 0 && img.x + img.width > ctx.window.width) {
        ctx.image.x = 0;
    }
    if (img.y > 0 && img.y + img.height > ctx.window.height) {
        ctx.image.y = 0;
    }
    if (img.x < 0 && img.x + img.width < ctx.window.width) {
        ctx.image.x = ctx.window.width - img.width;
    }
    if (img.y < 0 && img.y + img.height < ctx.window.height) {
        ctx.image.y = ctx.window.height - img.height;
    }
    if (img.width <= ctx.window.width) {
        ctx.image.x = ctx.window.width / 2 - img.width / 2;
    }
    if (img.height <= ctx.window.height) {
        ctx.image.y = ctx.window.height / 2 - img.height / 2;
    }
}

bool canvas_reset_window(size_t width, size_t height, size_t scale)
{
    const bool first = (ctx.window.width == 0);

    ctx.window.width = width;
    ctx.window.height = height;

    ctx.wnd_scale = scale;
    font_scale(scale);

    fix_viewport();

    return first;
}

void canvas_reset_image(size_t width, size_t height, enum canvas_scale sc)
{
    ctx.image.x = 0;
    ctx.image.y = 0;
    ctx.image.width = width;
    ctx.image.height = height;
    ctx.scale = 0;
    canvas_set_scale(sc);
}

void canvas_swap_image_size(void)
{
    const ssize_t diff = (ssize_t)ctx.image.width - ctx.image.height;
    const ssize_t shift = (ctx.scale * diff) / 2;
    const size_t old_width = ctx.image.width;

    ctx.image.x += shift;
    ctx.image.y -= shift;
    ctx.image.width = ctx.image.height;
    ctx.image.height = old_width;

    fix_viewport();
}

void canvas_clear(argb_t* wnd)
{
    if (config.window == COLOR_TRANSPARENT) {
        memset(wnd, 0, ctx.window.width * ctx.window.height * sizeof(argb_t));
    } else {
        for (size_t y = 0; y < ctx.window.height; ++y) {
            argb_t* line = &wnd[y * ctx.window.width];
            for (size_t x = 0; x < ctx.window.width; ++x) {
                line[x] = ARGB_SET_A(0xff) | config.window;
            }
        }
    }
}

/**
 * Draw image on canvas (bicubic filter).
 * @param vp destination viewport
 * @param img buffer with image data
 * @param wnd window buffer
 */
static void canvas_draw_bicubic(const struct rect* vp, const argb_t* img,
                                argb_t* wnd)
{
    size_t state_zero_x = 1;
    size_t state_zero_y = 1;
    float state[4][4][4]; // color channel, y, x

    for (size_t y = 0; y < vp->height; ++y) {
        argb_t* wnd_line = &wnd[(vp->y + y) * ctx.window.width + vp->x];
        const float scaled_y =
            (float)(y + vp->y - ctx.image.y) / ctx.scale - 0.5;
        const size_t img_y = (size_t)scaled_y;
        const float diff_y = scaled_y - img_y;
        const float diff_y2 = diff_y * diff_y;
        const float diff_y3 = diff_y * diff_y2;

        for (size_t x = 0; x < vp->width; ++x) {
            const float scaled_x =
                (float)(x + vp->x - ctx.image.x) / ctx.scale - 0.5;
            const size_t img_x = (size_t)scaled_x;
            const float diff_x = scaled_x - img_x;
            const float diff_x2 = diff_x * diff_x;
            const float diff_x3 = diff_x * diff_x2;
            argb_t fg = 0;

            // update cached state
            if (state_zero_x != img_x || state_zero_y != img_y) {
                float pixels[4][4][4]; // color channel, y, x
                state_zero_x = img_x;
                state_zero_y = img_y;
                for (size_t pc = 0; pc < 4; ++pc) {
                    // get colors for the current area
                    for (size_t py = 0; py < 4; ++py) {
                        size_t iy = img_y + py;
                        if (iy > 0) {
                            --iy;
                            if (iy >= ctx.image.height) {
                                iy = ctx.image.height - 1;
                            }
                        }
                        for (size_t px = 0; px < 4; ++px) {
                            size_t ix = img_x + px;
                            if (ix > 0) {
                                --ix;
                                if (ix >= ctx.image.width) {
                                    ix = ctx.image.width - 1;
                                }
                            }
                            const argb_t pixel = img[iy * ctx.image.width + ix];
                            pixels[pc][py][px] = (pixel >> (pc * 8)) & 0xff;
                        }
                    }
                    // recalc state cache for the current area
                    // clang-format off
                    state[pc][0][0] = pixels[pc][1][1];
                    state[pc][0][1] = -0.5 * pixels[pc][1][0] + 0.5  * pixels[pc][1][2];
                    state[pc][0][2] =        pixels[pc][1][0] - 2.5  * pixels[pc][1][1] + 2.0  * pixels[pc][1][2] - 0.5  * pixels[pc][1][3];
                    state[pc][0][3] = -0.5 * pixels[pc][1][0] + 1.5  * pixels[pc][1][1] - 1.5  * pixels[pc][1][2] + 0.5  * pixels[pc][1][3];
                    state[pc][1][0] = -0.5 * pixels[pc][0][1] + 0.5  * pixels[pc][2][1];
                    state[pc][1][1] = 0.25 * pixels[pc][0][0] - 0.25 * pixels[pc][0][2] -
                                      0.25 * pixels[pc][2][0] + 0.25 * pixels[pc][2][2];
                    state[pc][1][2] = -0.5 * pixels[pc][0][0] + 1.25 * pixels[pc][0][1] -        pixels[pc][0][2] + 0.25 * pixels[pc][0][3] +
                                       0.5 * pixels[pc][2][0] - 1.25 * pixels[pc][2][1] +        pixels[pc][2][2] - 0.25 * pixels[pc][2][3];
                    state[pc][1][3] = 0.25 * pixels[pc][0][0] - 0.75 * pixels[pc][0][1] + 0.75 * pixels[pc][0][2] - 0.25 * pixels[pc][0][3] -
                                      0.25 * pixels[pc][2][0] + 0.75 * pixels[pc][2][1] - 0.75 * pixels[pc][2][2] + 0.25 * pixels[pc][2][3];
                    state[pc][2][0] =        pixels[pc][0][1] - 2.5  * pixels[pc][1][1] + 2.0  * pixels[pc][2][1] - 0.5  * pixels[pc][3][1];
                    state[pc][2][1] = -0.5 * pixels[pc][0][0] + 0.5  * pixels[pc][0][2] + 1.25 * pixels[pc][1][0] - 1.25 * pixels[pc][1][2] -
                                             pixels[pc][2][0] +        pixels[pc][2][2] + 0.25 * pixels[pc][3][0] - 0.25 * pixels[pc][3][2];
                    state[pc][2][2] =        pixels[pc][0][0] - 2.5  * pixels[pc][0][1] + 2.0  * pixels[pc][0][2] - 0.5  * pixels[pc][0][3] -
                                       2.5 * pixels[pc][1][0] + 6.25 * pixels[pc][1][1] - 5.0  * pixels[pc][1][2] + 1.25 * pixels[pc][1][3] +
                                       2.0 * pixels[pc][2][0] - 5.0  * pixels[pc][2][1] + 4.0  * pixels[pc][2][2] -        pixels[pc][2][3] -
                                       0.5 * pixels[pc][3][0] + 1.25 * pixels[pc][3][1] -        pixels[pc][3][2] + 0.25 * pixels[pc][3][3];
                    state[pc][2][3] = -0.5 * pixels[pc][0][0] + 1.5  * pixels[pc][0][1] - 1.5  * pixels[pc][0][2] + 0.5  * pixels[pc][0][3] +
                                      1.25 * pixels[pc][1][0] - 3.75 * pixels[pc][1][1] + 3.75 * pixels[pc][1][2] - 1.25 * pixels[pc][1][3] -
                                             pixels[pc][2][0] + 3.0  * pixels[pc][2][1] - 3.0  * pixels[pc][2][2] +        pixels[pc][2][3] +
                                      0.25 * pixels[pc][3][0] - 0.75 * pixels[pc][3][1] + 0.75 * pixels[pc][3][2] - 0.25 * pixels[pc][3][3];
                    state[pc][3][0] = -0.5 * pixels[pc][0][1] + 1.5  * pixels[pc][1][1] - 1.5  * pixels[pc][2][1] + 0.5  * pixels[pc][3][1];
                    state[pc][3][1] = 0.25 * pixels[pc][0][0] - 0.25 * pixels[pc][0][2] -
                                      0.75 * pixels[pc][1][0] + 0.75 * pixels[pc][1][2] +
                                      0.75 * pixels[pc][2][0] - 0.75 * pixels[pc][2][2] -
                                      0.25 * pixels[pc][3][0] + 0.25 * pixels[pc][3][2];
                    state[pc][3][2] = -0.5 * pixels[pc][0][0] + 1.25 * pixels[pc][0][1] -        pixels[pc][0][2] + 0.25 * pixels[pc][0][3] +
                                       1.5 * pixels[pc][1][0] - 3.75 * pixels[pc][1][1] + 3.0  * pixels[pc][1][2] - 0.75 * pixels[pc][1][3] -
                                       1.5 * pixels[pc][2][0] + 3.75 * pixels[pc][2][1] - 3.0  * pixels[pc][2][2] + 0.75 * pixels[pc][2][3] +
                                       0.5 * pixels[pc][3][0] - 1.25 * pixels[pc][3][1] +        pixels[pc][3][2] - 0.25 * pixels[pc][3][3];
                    state[pc][3][3] = 0.25 * pixels[pc][0][0] - 0.75 * pixels[pc][0][1] + 0.75 * pixels[pc][0][2] - 0.25 * pixels[pc][0][3] -
                                      0.75 * pixels[pc][1][0] + 2.25 * pixels[pc][1][1] - 2.25 * pixels[pc][1][2] + 0.75 * pixels[pc][1][3] +
                                      0.75 * pixels[pc][2][0] - 2.25 * pixels[pc][2][1] + 2.25 * pixels[pc][2][2] - 0.75 * pixels[pc][2][3] -
                                      0.25 * pixels[pc][3][0] + 0.75 * pixels[pc][3][1] - 0.75 * pixels[pc][3][2] + 0.25 * pixels[pc][3][3];
                    // clang-format on
                }
            }

            // set pixel
            for (size_t pc = 0; pc < 4; ++pc) {
                // clang-format off
                const float inter =
                    (state[pc][0][0] + state[pc][0][1] * diff_x + state[pc][0][2] * diff_x2 + state[pc][0][3] * diff_x3) +
                    (state[pc][1][0] + state[pc][1][1] * diff_x + state[pc][1][2] * diff_x2 + state[pc][1][3] * diff_x3) * diff_y +
                    (state[pc][2][0] + state[pc][2][1] * diff_x + state[pc][2][2] * diff_x2 + state[pc][2][3] * diff_x3) * diff_y2 +
                    (state[pc][3][0] + state[pc][3][1] * diff_x + state[pc][3][2] * diff_x2 + state[pc][3][3] * diff_x3) * diff_y3;
                // clang-format on
                const uint8_t color = max(min(inter, 255), 0);
                fg |= (color << (pc * 8));
            }

            wnd_line[x] = fg;
        }
    }
}

void canvas_draw_image(bool alpha, const argb_t* img, argb_t* wnd)
{
    const ssize_t scaled_x = ctx.image.x + ctx.scale * ctx.image.width;
    const ssize_t scaled_y = ctx.image.y + ctx.scale * ctx.image.height;
    const ssize_t pos_left = max(0, ctx.image.x);
    const ssize_t pos_top = max(0, ctx.image.y);
    const ssize_t pos_right = min((ssize_t)ctx.window.width, scaled_x);
    const ssize_t pos_bottom = min((ssize_t)ctx.window.height, scaled_y);
    // intersection between window and image
    const struct rect viewport = { .x = pos_left,
                                   .y = pos_top,
                                   .width = pos_right - pos_left,
                                   .height = pos_bottom - pos_top };

    if (config.antialiasing) {
        canvas_draw_bicubic(&viewport, img, wnd);
    } else {
        for (size_t y = 0; y < viewport.height; ++y) {
            argb_t* wnd_line =
                &wnd[(viewport.y + y) * ctx.window.width + viewport.x];
            const size_t img_y =
                (float)(y + viewport.y - ctx.image.y) / ctx.scale;
            for (size_t x = 0; x < viewport.width; ++x) {
                const size_t img_x =
                    (float)(x + viewport.x - ctx.image.x) / ctx.scale;
                argb_t fg = img[img_y * ctx.image.width + img_x];
                wnd_line[x] = fg;
            }
        }
    }

    if (alpha) {
        for (size_t y = 0; y < viewport.height; ++y) {
            argb_t* wnd_line =
                &wnd[(viewport.y + y) * ctx.window.width + viewport.x];
            for (size_t x = 0; x < viewport.width; ++x) {
                argb_t fg;
                fg = wnd_line[x];

                // alpha blending
                const uint8_t alpha = ARGB_GET_A(fg);
                uint8_t alpha_set;
                argb_t bg;

                if (config.background == COLOR_TRANSPARENT) {
                    bg = 0;
                    alpha_set = alpha;
                } else if (config.background == BACKGROUND_GRID) {
                    const bool shift = (y / (GRID_STEP * ctx.wnd_scale)) % 2;
                    const size_t tail = x / (GRID_STEP * ctx.wnd_scale);
                    const argb_t grid =
                        (tail % 2) ^ shift ? GRID_COLOR1 : GRID_COLOR2;
                    bg = grid;
                    alpha_set = 0xff;
                } else {
                    bg = config.background;
                    alpha_set = 0xff;
                }

                wnd_line[x] = ARGB_ALPHA_BLEND(alpha, alpha_set, bg, fg);
            }
        }
    }
}

void canvas_print_line(argb_t* wnd, enum canvas_corner corner, const char* text)
{
    struct point pos = { 0, 0 };

    switch (corner) {
        case cc_top_right:
            pos.x = ctx.window.width - font_text_width(text, 0) - TEXT_PADDING;
            pos.y = TEXT_PADDING;
            break;
        case cc_bottom_left:
            pos.x = TEXT_PADDING;
            pos.y = ctx.window.height - font_height() - TEXT_PADDING;
            break;
        case cc_bottom_right:
            pos.x = ctx.window.width - font_text_width(text, 0) - TEXT_PADDING;
            pos.y = ctx.window.height - font_height() - TEXT_PADDING;
            break;
    }

    font_print(wnd, &ctx.window, &pos, text, 0);
}

void canvas_print_info(argb_t* wnd, size_t num, const struct info_table* info)
{
    size_t val_offset = 0;
    struct point pos = { .x = TEXT_PADDING, .y = TEXT_PADDING };

    // draw keys block
    for (size_t i = 0; i < num; ++i) {
        const char* key = info[i].key;
        if (key && *key) {
            size_t width;
            struct point pos_delim;
            width = font_print(wnd, &ctx.window, &pos, key, 0);
            pos_delim.x = TEXT_PADDING + width;
            pos_delim.y = pos.y;
            width += font_print(wnd, &ctx.window, &pos_delim, ":", 0) * 3;
            if (width > val_offset) {
                val_offset = width;
            }
        }
        pos.y += font_height();
    }

    // draw values block
    pos.x = TEXT_PADDING + val_offset + TEXT_PADDING;
    pos.y = TEXT_PADDING;
    for (size_t i = 0; i < num; ++i) {
        const char* value = info[i].value;
        if (value && *value) {
            font_print(wnd, &ctx.window, &pos, value, 0);
        }
        pos.y += font_height();
    }
}

bool canvas_move(enum canvas_move mv)
{
    const size_t scaled_width = ctx.scale * ctx.image.width;
    const size_t scaled_height = ctx.scale * ctx.image.height;
    const size_t step_x = ctx.window.width / 10;
    const size_t step_y = ctx.window.height / 10;
    ssize_t prev_x = ctx.image.x;
    ssize_t prev_y = ctx.image.y;

    switch (mv) {
        case cm_center:
            ctx.image.x = ctx.window.width / 2 - scaled_width / 2;
            ctx.image.y = ctx.window.height / 2 - scaled_height / 2;
            break;
        case cm_cnt_hor:
            ctx.image.x = ctx.window.width / 2 - scaled_width / 2;
            break;
        case cm_cnt_vert:
            ctx.image.y = ctx.window.height / 2 - scaled_height / 2;
            break;
        case cm_step_left:
            ctx.image.x += step_x;
            break;
        case cm_step_right:
            ctx.image.x -= step_x;
            break;
        case cm_step_up:
            ctx.image.y += step_y;
            break;
        case cm_step_down:
            ctx.image.y -= step_y;
            break;
    }

    fix_viewport();

    return (ctx.image.x != prev_x || ctx.image.y != prev_y);
}

void canvas_set_scale(enum canvas_scale sc)
{
    const float prev = ctx.scale;

    // set new scale factor
    if (sc == cs_fit_or100 || sc == cs_fit_window || sc == cs_fill_window) {
        const float sw = 1.0 / ((float)ctx.image.width / ctx.window.width);
        const float sh = 1.0 / ((float)ctx.image.height / ctx.window.height);
        if (sc == cs_fill_window) {
            ctx.scale = max(sw, sh);
        } else {
            ctx.scale = min(sw, sh);
            if (sc == cs_fit_or100 && ctx.scale > 1.0) {
                ctx.scale = 1.0;
            }
        }
    } else if (sc == cs_real_size) {
        ctx.scale = 1.0; // 100 %
    } else {
        const float step = ctx.scale / 10.0;
        if (sc == cs_zoom_in) {
            ctx.scale += step;
            if (ctx.scale > MAX_SCALE) {
                ctx.scale = MAX_SCALE;
            }
        } else if (sc == cs_zoom_out) {
            const float sw = (float)MIN_SCALE / ctx.image.width;
            const float sh = (float)MIN_SCALE / ctx.image.height;
            const float scale_min = max(sw, sh);
            ctx.scale -= step;
            if (ctx.scale < scale_min) {
                ctx.scale = scale_min;
            }
        }
    }

    // move viewport
    if (sc != cs_zoom_in && sc != cs_zoom_out) {
        canvas_move(cm_center);
    } else {
        // move to save the center of previous coordinates
        const size_t old_w = prev * ctx.image.width;
        const size_t old_h = prev * ctx.image.height;
        const size_t new_w = ctx.scale * ctx.image.width;
        const size_t new_h = ctx.scale * ctx.image.height;
        const ssize_t delta_w = old_w - new_w;
        const ssize_t delta_h = old_h - new_h;
        const ssize_t cntr_x = ctx.window.width / 2 - ctx.image.x;
        const ssize_t cntr_y = ctx.window.height / 2 - ctx.image.y;
        ctx.image.x += ((float)cntr_x / old_w) * delta_w;
        ctx.image.y += ((float)cntr_y / old_h) * delta_h;
        fix_viewport();
    }
}

float canvas_get_scale(void)
{
    return ctx.scale;
}
