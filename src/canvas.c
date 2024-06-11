// SPDX-License-Identifier: MIT
// Canvas used to render images and text to window buffer.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "canvas.h"

#include "config.h"
#include "str.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

// Background modes
#define COLOR_TRANSPARENT 0xff000000
#define BACKGROUND_GRID   0xfe000000

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

    bool fixed; ///< Fix canvas position

    enum canvas_scale initial_scale; ///< Initial scale
    float scale;                     ///< Current scale factor of the image

    struct rect image; ///< Image position and size
};

static struct canvas ctx = {
    .image_bkg = BACKGROUND_GRID,
    .window_bkg = COLOR_TRANSPARENT,
    .fixed = true,
};

/**
 * Fix canvas position.
 */
static void fix_position(bool force)
{
    const ssize_t wnd_width = ui_get_width();
    const ssize_t wnd_height = ui_get_height();
    const ssize_t img_width = ctx.scale * ctx.image.width;
    const ssize_t img_height = ctx.scale * ctx.image.height;

    if (force || ctx.fixed) {
        // bind to window border
        if (ctx.image.x > 0 && ctx.image.x + img_width > wnd_width) {
            ctx.image.x = 0;
        }
        if (ctx.image.y > 0 && ctx.image.y + img_height > wnd_height) {
            ctx.image.y = 0;
        }
        if (ctx.image.x < 0 && ctx.image.x + img_width < wnd_width) {
            ctx.image.x = wnd_width - img_width;
        }
        if (ctx.image.y < 0 && ctx.image.y + img_height < wnd_height) {
            ctx.image.y = wnd_height - img_height;
        }

        // centering small image
        if (img_width <= wnd_width) {
            ctx.image.x = wnd_width / 2 - img_width / 2;
        }
        if (img_height <= wnd_height) {
            ctx.image.y = wnd_height / 2 - img_height / 2;
        }
    }

    // don't let canvas to be far out of window
    if (ctx.image.x + img_width < 0) {
        ctx.image.x = -img_width;
    }
    if (ctx.image.x > wnd_width) {
        ctx.image.x = wnd_width;
    }
    if (ctx.image.y + img_height < 0) {
        ctx.image.y = -img_height;
    }
    if (ctx.image.y > wnd_height) {
        ctx.image.y = wnd_height;
    }
}

/**
 * Set fixed scale for the image.
 * @param sc scale to set
 */
static void set_scale(enum canvas_scale sc)
{
    const size_t wnd_width = ui_get_width();
    const size_t wnd_height = ui_get_height();
    const float scale_w = 1.0 / ((float)ctx.image.width / wnd_width);
    const float scale_h = 1.0 / ((float)ctx.image.height / wnd_height);

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
    ctx.image.x = wnd_width / 2 - (ctx.scale * ctx.image.width) / 2;
    ctx.image.y = wnd_height / 2 - (ctx.scale * ctx.image.height) / 2;

    fix_position(true);
}

/**
 * Zoom in/out.
 * @param percent percentage increment to current scale
 */
static void zoom(ssize_t percent)
{
    const double wnd_half_w = ui_get_width() / 2;
    const double wnd_half_h = ui_get_height() / 2;
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

    fix_position(false);
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
    } else if (strcmp(key, CANVAS_CFG_FIXED) == 0) {
        if (config_to_bool(value, &ctx.fixed)) {
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
}

void canvas_reset_window(void)
{
    fix_position(true);
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

    fix_position(false);
}

void canvas_draw_image(struct pixmap* wnd, const struct image* img,
                       size_t frame)
{
    const struct pixmap* pm = &img->frames[frame].pm;
    const size_t width = ctx.scale * ctx.image.width;
    const size_t height = ctx.scale * ctx.image.height;

    // clear window background
    const argb_t wnd_color = (ctx.window_bkg == COLOR_TRANSPARENT
                                  ? 0
                                  : ARGB_SET_A(0xff) | ctx.window_bkg);
    pixmap_inverse_fill(wnd, ctx.image.x, ctx.image.y, width, height,
                        wnd_color);

    // clear image background
    if (img->alpha) {
        if (ctx.image_bkg == BACKGROUND_GRID) {
            pixmap_grid(wnd, ctx.image.x, ctx.image.y, width, height,
                        ui_get_scale() * GRID_STEP, GRID_COLOR1, GRID_COLOR2);
        } else {
            const argb_t color = (ctx.image_bkg == COLOR_TRANSPARENT
                                      ? wnd_color
                                      : ARGB_SET_A(0xff) | ctx.image_bkg);
            pixmap_fill(wnd, ctx.image.x, ctx.image.y, width, height, color);
        }
    }

    // put image on window surface
    pixmap_put(wnd, pm, ctx.image.x, ctx.image.y, ctx.scale, img->alpha,
               ctx.scale == 1.0 ? false : ctx.antialiasing);
}

bool canvas_move(bool horizontal, ssize_t percent)
{
    const ssize_t old_x = ctx.image.x;
    const ssize_t old_y = ctx.image.y;

    if (horizontal) {
        ctx.image.x += (ui_get_width() / 100) * percent;
    } else {
        ctx.image.y += (ui_get_height() / 100) * percent;
    }

    fix_position(false);

    return (ctx.image.x != old_x || ctx.image.y != old_y);
}

bool canvas_drag(int dx, int dy)
{
    const ssize_t old_x = ctx.image.x;
    const ssize_t old_y = ctx.image.y;

    ctx.image.x += dx;
    ctx.image.y += dy;

    fix_position(false);

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
