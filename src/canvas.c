// SPDX-License-Identifier: MIT
// Canvas used to render images and text to window buffer.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "canvas.h"

#include "config.h"
#include "str.h"

#include <stdio.h>
#include <string.h>

// Background modes
#define COLOR_TRANSPARENT 0xff000000
#define BACKGROUND_GRID   0xfe000000

// Text rendering parameters
#define TEXT_COLOR     0x00cccccc
#define TEXT_SHADOW    0x00000000
#define TEXT_NO_SHADOW 0xff000000
#define TEXT_PADDING   10 // space between text layout and window edge
#define TEXT_LINESP    4  // line spacing factor

// Background grid parameters
#define GRID_STEP   10
#define GRID_COLOR1 0xff333333
#define GRID_COLOR2 0xff4c4c4c

// Scale thresholds
#define MIN_SCALE 10    // pixels
#define MAX_SCALE 100.0 // factor

#define max(x, y) ((x) > (y) ? (x) : (y))
#define min(x, y) ((x) < (y) ? (x) : (y))

/** Scaling operations. */
enum canvas_scale {
    scale_fit_optimal, ///< Fit to window, but not more than 100%
    scale_fit_window,  ///< Fit to window size
    scale_fit_width,   ///< Fit width to window width
    scale_fit_height,  ///< Fit height to window height
    scale_fill_window, ///< Fill the window
    scale_real_size,   ///< Real image size (100%)
};

// clang-format off
static const char* scale_names[] = {
    [scale_fit_optimal] = "optimal",
    [scale_fit_window] = "fit",
    [scale_fit_width] = "width",
    [scale_fit_height] = "height",
    [scale_fill_window] = "fill",
    [scale_real_size] = "real",
};
// clang-format on

/** Canvas context. */
struct canvas {
    argb_t image_bkg;  ///< Image background mode/color
    argb_t window_bkg; ///< Window background mode/color
    bool antialiasing; ///< Anti-aliasing (bicubic interpolation)

    argb_t font_color;  ///< Font color
    argb_t font_shadow; ///< Font shadow color

    enum canvas_scale initial_scale; ///< Initial scale
    float scale;                     ///< Current scale factor

    struct rect image;  ///< Image position and size
    struct size window; ///< Output window size
    size_t wnd_scale;   ///< Window scale factor (HiDPI)
};

static struct canvas ctx = {
    .image_bkg = BACKGROUND_GRID,
    .window_bkg = COLOR_TRANSPARENT,
    .font_color = TEXT_COLOR,
    .font_shadow = TEXT_SHADOW,
};

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

/**
 * Set fixed scale for the image.
 * @param sc scale to set
 */
static void set_scale(enum canvas_scale sc)
{
    const float scale_w = 1.0 / ((float)ctx.image.width / ctx.window.width);
    const float scale_h = 1.0 / ((float)ctx.image.height / ctx.window.height);

    switch (sc) {
        case scale_fit_optimal:
            ctx.scale = min(scale_w, scale_h);
            if (ctx.scale > 1.0) {
                ctx.scale = 1.0;
            }
            break;
        case scale_fit_window:
            ctx.scale = min(scale_w, scale_h);
            break;
        case scale_fit_width:
            ctx.scale = scale_w;
            break;
        case scale_fit_height:
            ctx.scale = scale_h;
            break;
        case scale_fill_window:
            ctx.scale = max(scale_w, scale_h);
            break;
        case scale_real_size:
            ctx.scale = 1.0; // 100 %
            break;
    }

    // center viewport
    ctx.image.x = ctx.window.width / 2 - (ctx.scale * ctx.image.width) / 2;
    ctx.image.y = ctx.window.height / 2 - (ctx.scale * ctx.image.height) / 2;

    fix_viewport();
}

/**
 * Zoom in/out.
 * @param percent percentage increment to current scale
 */
static void zoom(ssize_t percent)
{
    const double wnd_half_w = ctx.window.width / 2;
    const double wnd_half_h = ctx.window.height / 2;
    const float step = (ctx.scale / 100) * percent;

    // get current center
    const double center_x = wnd_half_w / ctx.scale - ctx.image.x / ctx.scale;
    const double center_y = wnd_half_h / ctx.scale - ctx.image.y / ctx.scale;

    if (percent > 0) {
        ctx.scale += step;
        if (ctx.scale > MAX_SCALE) {
            ctx.scale = MAX_SCALE;
        }
    } else {
        const float scale_w = (float)MIN_SCALE / ctx.image.width;
        const float scale_h = (float)MIN_SCALE / ctx.image.height;
        const float scale_min = max(scale_w, scale_h);
        ctx.scale += step;
        if (ctx.scale < scale_min) {
            ctx.scale = scale_min;
        }
    }

    // restore center
    ctx.image.x = wnd_half_w - center_x * ctx.scale;
    ctx.image.y = wnd_half_h - center_y * ctx.scale;

    fix_viewport();
}

/**
 * Draw text surface on window.
 * @param wnd destination window
 * @param x,y text position
 * @param text text surface to draw
 */
static void draw_text(struct pixmap* wnd, size_t x, size_t y,
                      const struct text_surface* text)
{
    if (ctx.font_shadow != TEXT_NO_SHADOW) {
        size_t shadow_offset = text->height / 16;
        if (shadow_offset < 1) {
            shadow_offset = 1;
        }
        pixmap_apply_mask(wnd, x + shadow_offset, y + shadow_offset, text->data,
                          text->width, text->height, ctx.font_shadow);
    }

    pixmap_apply_mask(wnd, x, y, text->data, text->width, text->height,
                      ctx.font_color);
}

/**
 * Custom section loader, see `config_loader` for details.
 */
static enum config_status load_config(const char* key, const char* value)
{
    enum config_status status = cfgst_invalid_value;

    if (strcmp(key, CANVAS_CFG_ANTIALIASING) == 0) {
        if (config_to_bool(value, &ctx.antialiasing)) {
            status = cfgst_ok;
        }
    } else if (strcmp(key, CANVAS_CFG_SCALE) == 0) {
        const ssize_t index = str_index(scale_names, value, 0);
        if (index >= 0) {
            ctx.initial_scale = index;
            status = cfgst_ok;
        }
    } else if (strcmp(key, CANVAS_CFG_TRANSPARENCY) == 0) {
        if (strcmp(value, "grid") == 0) {
            ctx.image_bkg = BACKGROUND_GRID;
            status = cfgst_ok;
        } else if (strcmp(value, "none") == 0) {
            ctx.image_bkg = COLOR_TRANSPARENT;
            status = cfgst_ok;
        } else if (config_to_color(value, &ctx.image_bkg)) {
            status = cfgst_ok;
        }
    } else if (strcmp(key, CANVAS_CFG_BACKGROUND) == 0) {
        if (strcmp(value, "none") == 0) {
            ctx.window_bkg = COLOR_TRANSPARENT;
            status = cfgst_ok;
        } else if (config_to_color(value, &ctx.window_bkg)) {
            status = cfgst_ok;
        }
    } else {
        status = cfgst_invalid_key;
    }

    return status;
}

/**
 * Custom section loader, see `config_loader` for details.
 */
static enum config_status load_font_config(const char* key, const char* value)
{
    enum config_status status = cfgst_invalid_value;

    if (strcmp(key, "color") == 0) {
        if (config_to_color(value, &ctx.font_color)) {
            status = cfgst_ok;
        }
    } else if (strcmp(key, "shadow") == 0) {
        if (strcmp(value, "none") == 0) {
            ctx.font_shadow = TEXT_NO_SHADOW;
            status = cfgst_ok;
        } else if (config_to_color(value, &ctx.font_shadow)) {
            status = cfgst_ok;
        }
    } else {
        status = cfgst_invalid_key;
    }

    return status;
}

void canvas_init(void)
{
    // register configuration loader
    config_add_loader(GENERAL_CONFIG_SECTION, load_config);
    config_add_loader(FONT_CONFIG_SECTION, load_font_config);
}

bool canvas_reset_window(size_t width, size_t height, size_t scale)
{
    const bool first = (ctx.window.width == 0);

    ctx.window.width = width;
    ctx.window.height = height;

    ctx.wnd_scale = scale;
    font_set_scale(scale);

    fix_viewport();

    return first;
}

void canvas_reset_image(size_t width, size_t height)
{
    ctx.image.x = 0;
    ctx.image.y = 0;
    ctx.image.width = width;
    ctx.image.height = height;
    ctx.scale = 0;
    set_scale(ctx.initial_scale);
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

void canvas_draw_image(struct pixmap* wnd, const struct image* img,
                       size_t frame)
{
    const struct pixmap* pm = &img->frames[frame].pm;
    const ssize_t scaled_x = ctx.image.x + ctx.scale * pm->width;
    const ssize_t scaled_y = ctx.image.y + ctx.scale * pm->height;
    const ssize_t wnd_x0 = max(0, ctx.image.x);
    const ssize_t wnd_y0 = max(0, ctx.image.y);
    const ssize_t wnd_x1 = min((ssize_t)wnd->width, scaled_x);
    const ssize_t wnd_y1 = min((ssize_t)wnd->height, scaled_y);
    const size_t width = wnd_x1 - wnd_x0;
    const size_t height = wnd_y1 - wnd_y0;

    // clear window background
    const argb_t wnd_color = (ctx.window_bkg == COLOR_TRANSPARENT
                                  ? 0
                                  : ARGB_SET_A(0xff) | ctx.window_bkg);
    if (height < wnd->height) {
        pixmap_fill(wnd, 0, 0, wnd->width, wnd_y0, wnd_color);
        pixmap_fill(wnd, 0, wnd_y1, wnd->width, wnd->height - wnd_y1,
                    wnd_color);
    }
    if (width < wnd->width) {
        pixmap_fill(wnd, 0, wnd_y0, wnd_x0, height, wnd_color);
        pixmap_fill(wnd, wnd_x1, wnd_y0, wnd->width - wnd_x1, height,
                    wnd_color);
    }

    if (img->alpha) {
        // clear image background
        if (ctx.image_bkg == BACKGROUND_GRID) {
            pixmap_grid(wnd, wnd_x0, wnd_y0, width, height,
                        GRID_STEP * ctx.wnd_scale, GRID_COLOR1, GRID_COLOR2);
        } else {
            const argb_t color = (ctx.image_bkg == COLOR_TRANSPARENT
                                      ? wnd_color
                                      : ARGB_SET_A(0xff) | ctx.image_bkg);
            pixmap_fill(wnd, wnd_x0, wnd_y0, width, height, color);
        }
    }

    // put image on window surface
    pixmap_put(wnd, wnd_x0, wnd_y0, pm, ctx.image.x, ctx.image.y, ctx.scale,
               img->alpha, ctx.antialiasing);
}

void canvas_draw_text(struct pixmap* wnd, enum info_position pos,
                      const struct info_line* lines, size_t lines_num)
{
    size_t max_key_width = 0;
    const size_t height =
        lines[0].value.height + lines[0].value.height / TEXT_LINESP;

    // calc max width of keys, used if block on the left side
    for (size_t i = 0; i < lines_num; ++i) {
        if (lines[i].key.width > max_key_width) {
            max_key_width = lines[i].key.width;
        }
    }
    max_key_width += height / 2;

    // draw info block
    for (size_t i = 0; i < lines_num; ++i) {
        const struct text_surface* key = &lines[i].key;
        const struct text_surface* value = &lines[i].value;
        size_t y = 0;
        size_t x_key = 0;
        size_t x_val = 0;

        // calculate line position
        switch (pos) {
            case info_top_left:
                y = TEXT_PADDING + i * height;
                if (key->data) {
                    x_key = TEXT_PADDING;
                    x_val = TEXT_PADDING + max_key_width;
                } else {
                    x_val = TEXT_PADDING;
                }
                break;
            case info_top_right:
                y = TEXT_PADDING + i * height;
                x_val = wnd->width - TEXT_PADDING - value->width;
                if (key->data) {
                    x_key = x_val - key->width - TEXT_PADDING;
                }
                break;
            case info_bottom_left:
                y = wnd->height - TEXT_PADDING - height * lines_num +
                    i * height;
                if (key->data) {
                    x_key = TEXT_PADDING;
                    x_val = TEXT_PADDING + max_key_width;
                } else {
                    x_val = TEXT_PADDING;
                }
                break;
            case info_bottom_right:
                y = wnd->height - TEXT_PADDING - height * lines_num +
                    i * height;
                x_val = wnd->width - TEXT_PADDING - value->width;
                if (key->data) {
                    x_key = x_val - key->width - TEXT_PADDING;
                }
                break;
        }

        if (key->data) {
            draw_text(wnd, x_key, y, key);
            x_key += key->width;
        }
        draw_text(wnd, x_val, y, value);
    }
}

void canvas_draw_ctext(struct pixmap* wnd, const struct text_surface* lines,
                       size_t lines_num)
{
    const size_t line_height = lines[0].height + lines[0].height / TEXT_LINESP;
    const size_t row_max = (wnd->height - TEXT_PADDING * 2) / line_height;
    const size_t columns =
        (lines_num / row_max) + (lines_num % row_max ? 1 : 0);
    const size_t rows = (lines_num / columns) + (lines_num % columns ? 1 : 0);
    const size_t col_space = line_height;
    size_t total_width = 0;
    size_t top = 0;
    size_t left = 0;

    // calculate total width
    for (size_t col = 0; col < columns; ++col) {
        size_t max_width = 0;
        for (size_t row = 0; row < rows; ++row) {
            const size_t index = row + col * rows;
            if (index >= lines_num) {
                break;
            }
            if (max_width < lines[index].width) {
                max_width = lines[index].width;
            }
        }
        total_width += max_width;
    }
    total_width += col_space * (columns - 1);

    // top left corner of the centered text block
    if (total_width < ctx.window.width) {
        left = wnd->width / 2 - total_width / 2;
    }
    if (rows * line_height < ctx.window.height) {
        top = wnd->height / 2 - (rows * line_height) / 2;
    }

    // put text on window
    for (size_t col = 0; col < columns; ++col) {
        size_t y = top;
        size_t col_width = 0;
        for (size_t row = 0; row < rows; ++row) {
            const size_t index = row + col * rows;
            if (index >= lines_num) {
                break;
            }
            draw_text(wnd, left, y, &lines[index]);
            if (col_width < lines[index].width) {
                col_width = lines[index].width;
            }
            y += line_height;
        }
        left += col_width + col_space;
    }
}

bool canvas_move(bool horizontal, ssize_t percent)
{
    const ssize_t old_x = ctx.image.x;
    const ssize_t old_y = ctx.image.y;

    if (horizontal) {
        ctx.image.x += (ctx.window.width / 100) * percent;
    } else {
        ctx.image.y += (ctx.window.height / 100) * percent;
    }

    fix_viewport();

    return (ctx.image.x != old_x || ctx.image.y != old_y);
}

bool canvas_drag(int dx, int dy)
{
    const ssize_t old_x = ctx.image.x;
    const ssize_t old_y = ctx.image.y;

    ctx.image.x += dx;
    ctx.image.y += dy;
    fix_viewport();

    return (ctx.image.x != old_x || ctx.image.y != old_y);
}

void canvas_zoom(const char* op)
{
    ssize_t percent = 0;

    if (!op || !*op) {
        return;
    }

    for (size_t i = 0; i < sizeof(scale_names) / sizeof(scale_names[0]); ++i) {
        if (strcmp(op, scale_names[i]) == 0) {
            set_scale(i);
            return;
        }
    }

    if (str_to_num(op, 0, &percent, 0) && percent != 0 && percent > -1000 &&
        percent < 1000) {
        zoom(percent);
        return;
    }

    fprintf(stderr, "Invalid zoom operation: \"%s\"\n", op);
}

float canvas_get_scale(void)
{
    return ctx.scale;
}

bool canvas_switch_aa(void)
{
    ctx.antialiasing = !ctx.antialiasing;
    return ctx.antialiasing;
}
