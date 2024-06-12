// SPDX-License-Identifier: MIT
// Business logic of application and UI event handlers.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "viewer.h"

#include "buildcfg.h"
#include "config.h"
#include "imagelist.h"
#include "info.h"
#include "keybind.h"
#include "str.h"
#include "text.h"
#include "ui.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timerfd.h>
#include <sys/wait.h>
#include <unistd.h>

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

/** Scaling operations. */
enum fixed_scale {
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

/** Viewer context. */
struct viewer {
    ssize_t img_x, img_y; ///< Top left corner of the image
    size_t frame;         ///< Index of the current frame
    argb_t image_bkg;     ///< Image background mode/color
    argb_t window_bkg;    ///< Window background mode/color
    bool antialiasing;    ///< Anti-aliasing mode on/off
    bool fixed;           ///< Fix image position

    enum fixed_scale scale_init; ///< Initial scale
    float scale;                 ///< Current scale factor of the image

    struct text_surface* help; ///< Help lines
    size_t help_sz;            ///< Number of lines in help

    bool animation_enable; ///< Animation enable/disable
    int animation_fd;      ///< Animation timer

    bool slideshow_enable; ///< Slideshow enable/disable
    int slideshow_fd;      ///< Slideshow timer
    size_t slideshow_time; ///< Slideshow image display time (seconds)
};

static struct viewer ctx = {
    .image_bkg = BACKGROUND_GRID,
    .window_bkg = COLOR_TRANSPARENT,
    .fixed = true,
    .animation_enable = true,
    .animation_fd = -1,
    .slideshow_enable = false,
    .slideshow_fd = -1,
    .slideshow_time = 3,
};

/**
 * Fix up image position.
 * @param force true to ignore current config setting
 */
static void fixup_position(bool force)
{
    const ssize_t wnd_width = ui_get_width();
    const ssize_t wnd_height = ui_get_height();

    const struct image_entry entry = image_list_current();
    const struct pixmap* img = &entry.image->frames[ctx.frame].pm;
    const ssize_t img_width = ctx.scale * img->width;
    const ssize_t img_height = ctx.scale * img->height;

    if (force || ctx.fixed) {
        // bind to window border
        if (ctx.img_x > 0 && ctx.img_x + img_width > wnd_width) {
            ctx.img_x = 0;
        }
        if (ctx.img_y > 0 && ctx.img_y + img_height > wnd_height) {
            ctx.img_y = 0;
        }
        if (ctx.img_x < 0 && ctx.img_x + img_width < wnd_width) {
            ctx.img_x = wnd_width - img_width;
        }
        if (ctx.img_y < 0 && ctx.img_y + img_height < wnd_height) {
            ctx.img_y = wnd_height - img_height;
        }

        // centering small image
        if (img_width <= wnd_width) {
            ctx.img_x = wnd_width / 2 - img_width / 2;
        }
        if (img_height <= wnd_height) {
            ctx.img_y = wnd_height / 2 - img_height / 2;
        }
    }

    // don't let canvas to be far out of window
    if (ctx.img_x + img_width < 0) {
        ctx.img_x = -img_width;
    }
    if (ctx.img_x > wnd_width) {
        ctx.img_x = wnd_width;
    }
    if (ctx.img_y + img_height < 0) {
        ctx.img_y = -img_height;
    }
    if (ctx.img_y > wnd_height) {
        ctx.img_y = wnd_height;
    }
}

/**
 * Move image (viewport).
 * @param horizontal axis along which to move (false for vertical)
 * @param positive direction (increase/decrease)
 * @param params optional move step in percents
 */
static bool move_image(bool horizontal, bool positive, const char* params)
{
    const ssize_t old_x = ctx.img_x;
    const ssize_t old_y = ctx.img_y;
    ssize_t step = 10; // in %

    if (params) {
        ssize_t val;
        if (str_to_num(params, 0, &val, 0) && val > 0 && val <= 1000) {
            step = val;
        } else {
            fprintf(stderr, "Invalid move step: \"%s\"\n", params);
        }
    }

    if (!positive) {
        step = -step;
    }

    if (horizontal) {
        ctx.img_x += (ui_get_width() / 100) * step;
    } else {
        ctx.img_y += (ui_get_height() / 100) * step;
    }

    fixup_position(false);

    return (ctx.img_x != old_x || ctx.img_y != old_y);
}

/**
 * Rotate image 90 degrees.
 * @param clockwise rotation direction
 */
static void rotate_image(bool clockwise)
{
    const struct image_entry entry = image_list_current();
    const struct pixmap* img = &entry.image->frames[ctx.frame].pm;
    const ssize_t diff = (ssize_t)img->width - img->height;
    const ssize_t shift = (ctx.scale * diff) / 2;

    image_rotate(entry.image, clockwise ? 90 : 270);
    ctx.img_x += shift;
    ctx.img_y -= shift;
    fixup_position(false);
}

/**
 * Set fixed scale for the image.
 * @param sc scale to set
 */
static void scale_image(enum fixed_scale sc)
{
    const struct image_entry entry = image_list_current();
    const struct pixmap* img = &entry.image->frames[ctx.frame].pm;
    const size_t wnd_width = ui_get_width();
    const size_t wnd_height = ui_get_height();
    const float scale_w = 1.0 / ((float)img->width / wnd_width);
    const float scale_h = 1.0 / ((float)img->height / wnd_height);

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
    ctx.img_x = wnd_width / 2 - (ctx.scale * img->width) / 2;
    ctx.img_y = wnd_height / 2 - (ctx.scale * img->height) / 2;

    fixup_position(true);
}

/**
 * Zoom in/out.
 * @param params zoom operation
 */
void zoom_image(const char* params)
{
    ssize_t percent = 0;
    ssize_t fixed_scale;

    if (!params || !*params) {
        return;
    }

    // check for fixed scale type
    fixed_scale = str_index(scale_names, params, 0);
    if (fixed_scale >= 0) {
        scale_image(fixed_scale);
    } else if (str_to_num(params, 0, &percent, 0) && percent != 0 &&
               percent > -1000 && percent < 1000) {
        // zoom in %
        const double wnd_half_w = ui_get_width() / 2;
        const double wnd_half_h = ui_get_height() / 2;
        const float step = (ctx.scale / 100) * percent;
        const double center_x = wnd_half_w / ctx.scale - ctx.img_x / ctx.scale;
        const double center_y = wnd_half_h / ctx.scale - ctx.img_y / ctx.scale;

        if (percent > 0) {
            ctx.scale += step;
            if (ctx.scale > MAX_SCALE) {
                ctx.scale = MAX_SCALE;
            }
        } else {
            const struct image_entry entry = image_list_current();
            const struct pixmap* img = &entry.image->frames[ctx.frame].pm;
            const float scale_w = (float)MIN_SCALE / img->width;
            const float scale_h = (float)MIN_SCALE / img->height;
            const float scale_min = max(scale_w, scale_h);
            ctx.scale += step;
            if (ctx.scale < scale_min) {
                ctx.scale = scale_min;
            }
        }

        // restore center
        ctx.img_x = wnd_half_w - center_x * ctx.scale;
        ctx.img_y = wnd_half_h - center_y * ctx.scale;
        fixup_position(false);
    } else {
        fprintf(stderr, "Invalid zoom operation: \"%s\"\n", params);
    }
}

/**
 * Start/stop animation if image supports it.
 * @param enable state to set
 */
static void animation_ctl(bool enable)
{
    struct itimerspec ts = { 0 };

    if (enable) {
        const struct image_entry entry = image_list_current();
        const size_t duration = entry.image->frames[ctx.frame].duration;
        enable = (entry.image->num_frames > 1 && duration);
        if (enable) {
            ts.it_value.tv_sec = duration / 1000;
            ts.it_value.tv_nsec = (duration % 1000) * 1000000;
        }
    }

    ctx.animation_enable = enable;
    timerfd_settime(ctx.animation_fd, 0, &ts, NULL);
}

/**
 * Start/stop slide show.
 * @param enable state to set
 */
static void slideshow_ctl(bool enable)
{
    struct itimerspec ts = { 0 };

    ctx.slideshow_enable = enable;
    if (enable) {
        ts.it_value.tv_sec = ctx.slideshow_time;
    }

    timerfd_settime(ctx.slideshow_fd, 0, &ts, NULL);
}

/**
 * Reset state to defaults.
 */
static void reset_state(void)
{
    const struct image_entry entry = image_list_current();

    ctx.frame = 0;
    ctx.img_x = 0;
    ctx.img_y = 0;
    ctx.scale = 0;
    scale_image(ctx.scale_init);
    fixup_position(true);

    ui_set_title(entry.image->file_name);
    animation_ctl(true);
    slideshow_ctl(ctx.slideshow_enable);
}

/**
 * Load next file.
 * @param jump position of the next file in list
 * @return false if file was not loaded
 */
static bool next_file(enum list_jump jump)
{
    if (!image_list_jump(jump)) {
        return false;
    }
    reset_state();
    return true;
}

/**
 * Switch to the next or previous frame.
 * @param forward switch direction
 * @return false if there is only one frame in the image
 */
static bool next_frame(bool forward)
{
    size_t index = ctx.frame;
    const struct image_entry entry = image_list_current();

    if (forward) {
        if (++index >= entry.image->num_frames) {
            index = 0;
        }
    } else {
        if (index-- == 0) {
            index = entry.image->num_frames - 1;
        }
    }
    if (index == ctx.frame) {
        return false;
    }

    ctx.frame = index;
    return true;
}

/**
 * Animation timer event handler.
 */
static void on_animation_timer(void)
{
    next_frame(true);
    animation_ctl(true);
    ui_redraw();
}

/**
 * Slideshow timer event handler.
 */
static void on_slideshow_timer(void)
{
    slideshow_ctl(next_file(jump_next_file));
    ui_redraw();
}

/**
 * Execute system command for the current image.
 * @param expr command expression
 */
static void execute_command(const char* expr)
{
    const char* path = image_list_current().image->file_path;
    char* cmd = NULL;
    int rc = EINVAL;

    // construct command from template
    while (expr && *expr) {
        if (*expr == '%') {
            ++expr;
            if (*expr != '%') {
                str_append(path, 0, &cmd); // replace % with path
                continue;
            }
        }
        str_append(expr, 1, &cmd);
        ++expr;
    }

    if (cmd) {
        rc = system(cmd); // execute
        if (rc != -1) {
            rc = WEXITSTATUS(rc);
        } else {
            rc = errno ? errno : EINVAL;
        }
    }

    // show execution status
    if (!cmd) {
        info_set_status("Error: no command to execute");
    } else {
        if (strlen(cmd) > 30) { // trim long command
            strcpy(&cmd[27], "...");
        }
        if (rc) {
            info_set_status("Error %d: %s", rc, cmd);
        } else {
            info_set_status("OK: %s", cmd);
        }
    }

    free(cmd);
}

/**
 * Show/hide help layer.
 */
static void switch_help(void)
{
    if (ctx.help) {
        for (size_t i = 0; i < ctx.help_sz; i++) {
            free(ctx.help[i].data);
        }
        free(ctx.help);
        ctx.help = NULL;
        ctx.help_sz = 0;
    } else {
        ctx.help_sz = 0;
        ctx.help = calloc(1, key_bindings_size * sizeof(struct text_surface));
        if (ctx.help) {
            for (size_t i = 0; i < key_bindings_size; i++) {
                if (key_bindings[i].help) {
                    font_render(key_bindings[i].help, &ctx.help[ctx.help_sz++]);
                }
            }
        }
    }
}

/**
 * Draw image.
 * @param wnd pixel map of target window
 */
static void draw_image(struct pixmap* wnd)
{
    const struct image_entry entry = image_list_current();
    const struct pixmap* img = &entry.image->frames[ctx.frame].pm;
    const size_t width = ctx.scale * img->width;
    const size_t height = ctx.scale * img->height;

    // clear window background
    const argb_t wnd_color = (ctx.window_bkg == COLOR_TRANSPARENT
                                  ? 0
                                  : ARGB_SET_A(0xff) | ctx.window_bkg);
    pixmap_inverse_fill(wnd, ctx.img_x, ctx.img_y, width, height, wnd_color);

    // clear image background
    if (entry.image->alpha) {
        if (ctx.image_bkg == BACKGROUND_GRID) {
            pixmap_grid(wnd, ctx.img_x, ctx.img_y, width, height,
                        ui_get_scale() * GRID_STEP, GRID_COLOR1, GRID_COLOR2);
        } else {
            const argb_t color = (ctx.image_bkg == COLOR_TRANSPARENT
                                      ? wnd_color
                                      : ARGB_SET_A(0xff) | ctx.image_bkg);
            pixmap_fill(wnd, ctx.img_x, ctx.img_y, width, height, color);
        }
    }

    // put image on window surface
    pixmap_put(wnd, img, ctx.img_x, ctx.img_y, ctx.scale, entry.image->alpha,
               ctx.scale == 1.0 ? false : ctx.antialiasing);
}

/**
 * Custom section loader, see `config_loader` for details.
 */
static enum config_status load_config(const char* key, const char* value)
{
    enum config_status status = cfgst_invalid_value;

    if (strcmp(key, VIEWER_CFG_ANTIALIASING) == 0) {
        if (config_to_bool(value, &ctx.antialiasing)) {
            status = cfgst_ok;
        }
    } else if (strcmp(key, VIEWER_CFG_SCALE) == 0) {
        const ssize_t index = str_index(scale_names, value, 0);
        if (index >= 0) {
            ctx.scale_init = index;
            status = cfgst_ok;
        }
    } else if (strcmp(key, VIEWER_CFG_TRANSPARENCY) == 0) {
        if (strcmp(value, "grid") == 0) {
            ctx.image_bkg = BACKGROUND_GRID;
            status = cfgst_ok;
        } else if (strcmp(value, "none") == 0) {
            ctx.image_bkg = COLOR_TRANSPARENT;
            status = cfgst_ok;
        } else if (config_to_color(value, &ctx.image_bkg)) {
            status = cfgst_ok;
        }
    } else if (strcmp(key, VIEWER_CFG_BACKGROUND) == 0) {
        if (strcmp(value, "none") == 0) {
            ctx.window_bkg = COLOR_TRANSPARENT;
            status = cfgst_ok;
        } else if (config_to_color(value, &ctx.window_bkg)) {
            status = cfgst_ok;
        }
    } else if (strcmp(key, VIEWER_CFG_FIXED) == 0) {
        if (config_to_bool(value, &ctx.fixed)) {
            status = cfgst_ok;
        }
    } else if (strcmp(key, VIEWER_CFG_SLIDESHOW) == 0) {
        if (config_to_bool(value, &ctx.slideshow_enable)) {
            status = cfgst_ok;
        }
    } else if (strcmp(key, VIEWER_CFG_SLIDESHOW_TIME) == 0) {
        ssize_t num;
        if (str_to_num(value, 0, &num, 0) && num != 0 && num <= 86400) {
            ctx.slideshow_time = num;
            status = cfgst_ok;
        }
    } else {
        status = cfgst_invalid_key;
    }

    return status;
}

void viewer_init(void)
{
    // setup animation timer
    ctx.animation_fd =
        timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (ctx.animation_fd != -1) {
        ui_add_event(ctx.animation_fd, on_animation_timer);
    }

    // setup slideshow timer
    ctx.slideshow_fd =
        timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (ctx.slideshow_fd != -1) {
        ui_add_event(ctx.slideshow_fd, on_slideshow_timer);
    }

    // register configuration loader
    config_add_loader(GENERAL_CONFIG_SECTION, load_config);
}

void viewer_free(void)
{
    for (size_t i = 0; i < ctx.help_sz; i++) {
        free(ctx.help[i].data);
    }
    if (ctx.animation_fd != -1) {
        close(ctx.animation_fd);
    }
    if (ctx.slideshow_fd != -1) {
        close(ctx.slideshow_fd);
    }
}

bool viewer_reload(void)
{
    if (!image_list_reset()) {
        printf("No more images, exit\n");
        ui_stop();
        return false;
    }
    reset_state();
    info_set_status("Image reloaded");
    ui_redraw();
    return true;
}

void viewer_on_redraw(struct pixmap* window)
{
    info_update(ctx.frame, ctx.scale);
    draw_image(window);

    // put text info blocks on window surface
    for (size_t i = 0; i < INFO_POSITION_NUM; ++i) {
        const size_t lines_num = info_height(i);
        if (lines_num) {
            const enum info_position pos = (enum info_position)i;
            const struct info_line* lines = info_lines(pos);
            text_print(window, pos, lines, lines_num);
        }
    }
    if (ctx.help) {
        text_print_centered(window, ctx.help, ctx.help_sz);
    }

    // reset one-time rendered notification message
    info_set_status(NULL);
}

void viewer_on_resize(void)
{
    fixup_position(false);
    reset_state();
}

void viewer_on_keyboard(xkb_keysym_t key, uint8_t mods)
{
    bool redraw = false;
    const struct action* action = keybind_actions(key, mods);

    if (!action) {
        char* name = keybind_name(key, mods);
        if (name) {
            info_set_status("Key %s is not bound", name);
            free(name);
            ui_redraw();
        }
        return;
    }

    // handle actions
    while (action->type != action_none) {
        switch (action->type) {
            case action_none:
                break;
            case action_help:
                switch_help();
                redraw = true;
                break;
            case action_first_file:
                redraw |= next_file(jump_first_file);
                break;
            case action_last_file:
                redraw |= next_file(jump_last_file);
                break;
            case action_prev_dir:
                redraw |= next_file(jump_prev_dir);
                break;
            case action_next_dir:
                redraw |= next_file(jump_next_dir);
                break;
            case action_prev_file:
                redraw |= next_file(jump_prev_file);
                break;
            case action_next_file:
                redraw |= next_file(jump_next_file);
                break;
            case action_skip_file:
                if (image_list_skip()) {
                    reset_state();
                    redraw = true;
                } else {
                    printf("No more images, exit\n");
                    goto stop;
                }
                break;
            case action_prev_frame:
            case action_next_frame:
                animation_ctl(false);
                redraw |= next_frame(action->type == action_next_frame);
                break;
            case action_animation:
                animation_ctl(!ctx.animation_enable);
                break;
            case action_slideshow:
                slideshow_ctl(!ctx.slideshow_enable &&
                              next_file(jump_next_file));
                redraw = true;
                break;
            case action_fullscreen:
                ui_toggle_fullscreen();
                break;
            case action_step_left:
                redraw |= move_image(true, true, action->params);
                break;
            case action_step_right:
                redraw |= move_image(true, false, action->params);
                break;
            case action_step_up:
                redraw |= move_image(false, true, action->params);
                break;
            case action_step_down:
                redraw |= move_image(false, false, action->params);
                break;
            case action_zoom:
                zoom_image(action->params);
                redraw = true;
                break;
            case action_rotate_left:
                rotate_image(false);
                redraw = true;
                break;
            case action_rotate_right:
                rotate_image(true);
                redraw = true;
                break;
            case action_flip_vertical:
                image_flip_vertical(image_list_current().image);
                redraw = true;
                break;
            case action_flip_horizontal:
                image_flip_horizontal(image_list_current().image);
                redraw = true;
                break;
            case action_antialiasing:
                ctx.antialiasing = !ctx.antialiasing;
                info_set_status("Anti-aliasing %s",
                                ctx.antialiasing ? "on" : "off");
                redraw = true;
                break;
            case action_reload:
                if (!viewer_reload()) {
                    goto stop;
                }
                break;
            case action_info:
                info_set_mode(action->params);
                redraw = true;
                break;
            case action_exec:
                execute_command(action->params);
                redraw = true;
                break;
            case action_status:
                info_set_status("%s", action->params);
                redraw = true;
                break;
            case action_exit:
                if (ctx.help) {
                    switch_help(); // remove help overlay
                    redraw = true;
                } else {
                    goto stop;
                }
                break;
        }
        ++action;
    }

    if (redraw) {
        ui_redraw();
    }
    return;

stop:
    ui_stop();
}

void viewer_on_drag(int dx, int dy)
{
    const ssize_t old_x = ctx.img_x;
    const ssize_t old_y = ctx.img_y;

    ctx.img_x += dx;
    ctx.img_y += dy;

    if (ctx.img_x != old_x || ctx.img_y != old_y) {
        fixup_position(false);
        ui_redraw();
    }
}
