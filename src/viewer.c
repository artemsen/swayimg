// SPDX-License-Identifier: MIT
// Business logic of application and UI event handlers.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "viewer.h"

#include "application.h"
#include "buildcfg.h"
#include "config.h"
#include "fetcher.h"
#include "imagelist.h"
#include "info.h"
#include "keybind.h"
#include "loader.h"
#include "str.h"
#include "text.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timerfd.h>
#include <unistd.h>

// Background grid parameters
#define GRID_BKGID  0x00f1f2f3
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

    bool info_timedout; ///< Indicates info block shouldn't be displayed anymore
    int info_timeout_fd; ///< Info timer

    size_t history; ///< Max number of cached images
    size_t preload; ///< Max number of images to preload
};

/** Global viewer context. */
static struct viewer ctx;

/**
 * Fix up image position.
 * @param force true to ignore current config setting
 */
static void fixup_position(bool force)
{
    const ssize_t wnd_width = ui_get_width();
    const ssize_t wnd_height = ui_get_height();

    const struct pixmap* img = &fetcher_current()->frames[ctx.frame].pm;
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
static void move_image(bool horizontal, bool positive, const char* params)
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

    if (ctx.img_x != old_x || ctx.img_y != old_y) {
        app_redraw();
    }
}

/**
 * Rotate image 90 degrees.
 * @param clockwise rotation direction
 */
static void rotate_image(bool clockwise)
{
    struct image* img = fetcher_current();
    const struct pixmap* pm = &img->frames[ctx.frame].pm;
    const ssize_t diff = (ssize_t)pm->width - pm->height;
    const ssize_t shift = (ctx.scale * diff) / 2;

    image_rotate(img, clockwise ? 90 : 270);
    ctx.img_x += shift;
    ctx.img_y -= shift;
    fixup_position(false);

    app_redraw();
}

/**
 * Set fixed scale for the image.
 * @param sc scale to set
 */
static void scale_image(enum fixed_scale sc)
{
    const struct image* img = fetcher_current();
    const struct pixmap* pm = &img->frames[ctx.frame].pm;
    const size_t wnd_width = ui_get_width();
    const size_t wnd_height = ui_get_height();
    const float scale_w = 1.0 / ((float)pm->width / wnd_width);
    const float scale_h = 1.0 / ((float)pm->height / wnd_height);

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
    ctx.img_x = wnd_width / 2 - (ctx.scale * pm->width) / 2;
    ctx.img_y = wnd_height / 2 - (ctx.scale * pm->height) / 2;

    fixup_position(true);
    info_update(info_scale, "%.0f%%", ctx.scale * 100);
    app_redraw();
}

/**
 * Zoom in/out.
 * @param params zoom operation
 */
static void zoom_image(const char* params)
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
            const struct image* img = fetcher_current();
            const struct pixmap* pm = &img->frames[ctx.frame].pm;
            const float scale_w = (float)MIN_SCALE / pm->width;
            const float scale_h = (float)MIN_SCALE / pm->height;
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

    info_update(info_scale, "%.0f%%", ctx.scale * 100);
    app_redraw();
}

/**
 * Start/stop animation if image supports it.
 * @param enable state to set
 */
static void animation_ctl(bool enable)
{
    struct itimerspec ts = { 0 };

    if (enable) {
        const struct image* img = fetcher_current();
        const size_t duration = img->frames[ctx.frame].duration;
        enable = (img->num_frames > 1 && duration);
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
    const struct image* img = fetcher_current();
    const int timeout = info_timeout();

    ctx.frame = 0;
    ctx.img_x = 0;
    ctx.img_y = 0;
    ctx.scale = 0;
    ctx.info_timedout = false;
    scale_image(ctx.scale_init);
    fixup_position(true);

    ui_set_title(img->name);
    animation_ctl(true);
    slideshow_ctl(ctx.slideshow_enable);

    info_reset(img);
    info_update(info_index, "%zu of %zu", img->index + 1, image_list_size());

    // Expire info block after timeout
    if (timeout != 0) {
        struct itimerspec info_ts = { 0 };
        if (timeout > 0) {
            // absolute time in sec
            info_ts.it_value.tv_sec = timeout;
        } else if (ctx.slideshow_enable) {
            // slideshow relative in %
            info_ts.it_value.tv_sec = ctx.slideshow_time * -timeout / 100;
        }
        timerfd_settime(ctx.info_timeout_fd, 0, &info_ts, NULL);
    }

    app_redraw();
}

/**
 * Skip current image.
 * @return true if next image was loaded
 */
static bool skip_image(void)
{
    size_t index = image_list_skip(fetcher_current()->index);

    while (index != IMGLIST_INVALID && !fetcher_open(index)) {
        index = image_list_next_file(index);
    }

    return (index != IMGLIST_INVALID);
}

/**
 * Switch to the next image.
 * @param direction next image position
 * @return true if next image was loaded
 */
static bool next_image(enum action_type direction)
{
    size_t index = fetcher_current()->index;

    do {
        switch (direction) {
            case action_first_file:
                index = image_list_first();
                break;
            case action_last_file:
                index = image_list_last();
                break;
            case action_prev_dir:
                index = image_list_prev_dir(index);
                break;
            case action_next_dir:
                index = image_list_next_dir(index);
                break;
            case action_prev_file:
                index = image_list_prev_file(index);
                break;
            case action_next_file:
                index = image_list_next_file(index);
                break;
            default:
                break;
        }
    } while (index != IMGLIST_INVALID && !fetcher_open(index));

    if (index == IMGLIST_INVALID) {
        return false;
    }

    reset_state();

    return true;
}

/**
 * Switch to the next or previous frame.
 * @param forward switch direction
 */
static void next_frame(bool forward)
{
    size_t index = ctx.frame;
    const struct image* img = fetcher_current();

    if (forward) {
        if (++index >= img->num_frames) {
            index = 0;
        }
    } else {
        if (index-- == 0) {
            index = img->num_frames - 1;
        }
    }
    if (index != ctx.frame) {
        ctx.frame = index;
        info_update(info_frame, "%zu of %zu", ctx.frame + 1, img->num_frames);
        info_update(info_image_size, "%zux%zu", img->frames[ctx.frame].pm.width,
                    img->frames[ctx.frame].pm.height);
        app_redraw();
    }
}

/**
 * Animation timer event handler.
 */
static void on_animation_timer(void)
{
    next_frame(true);
    animation_ctl(true);
}

/**
 * Slideshow timer event handler.
 */
static void on_slideshow_timer(void)
{
    slideshow_ctl(next_image(action_next_file));
}

/**
 * Info block timer event handler.
 */
static void on_info_block_timeout(void)
{
    // Reset timer to 0, so it won't fire again
    struct itimerspec info_ts = { 0 };
    timerfd_settime(ctx.info_timeout_fd, 0, &info_ts, NULL);
    ctx.info_timedout = true;
    app_redraw();
}

/**
 * Show/hide help layer.
 */
static void switch_help(void)
{
    const struct keybind* kb;

    app_redraw();

    if (ctx.help) {
        for (size_t i = 0; i < ctx.help_sz; i++) {
            free(ctx.help[i].data);
        }
        free(ctx.help);
        ctx.help = NULL;
        ctx.help_sz = 0;
        return;
    }

    // get number of bindings
    ctx.help_sz = 0;
    kb = keybind_all();
    while (kb) {
        if (kb->help) {
            ++ctx.help_sz;
        }
        kb = kb->next;
    }
    ctx.help = calloc(1, ctx.help_sz * sizeof(*ctx.help));
    if (ctx.help) {
        size_t i = ctx.help_sz - 1;
        kb = keybind_all();
        while (kb) {
            if (kb->help) {
                font_render(kb->help, &ctx.help[i--]);
            }
            kb = kb->next;
        }
    }
}

/**
 * Draw image.
 * @param wnd pixel map of target window
 */
static void draw_image(struct pixmap* wnd)
{
    const struct image* img = fetcher_current();
    const struct pixmap* img_pm = &img->frames[ctx.frame].pm;
    const size_t width = ctx.scale * img_pm->width;
    const size_t height = ctx.scale * img_pm->height;

    // clear window background
    pixmap_inverse_fill(wnd, ctx.img_x, ctx.img_y, width, height,
                        ctx.window_bkg);

    // clear image background
    if (img->alpha) {
        if (ctx.image_bkg == GRID_BKGID) {
            pixmap_grid(wnd, ctx.img_x, ctx.img_y, width, height,
                        ui_get_scale() * GRID_STEP, GRID_COLOR1, GRID_COLOR2);
        } else {
            pixmap_fill(wnd, ctx.img_x, ctx.img_y, width, height,
                        ctx.image_bkg);
        }
    }

    // put image on window surface
    if (ctx.scale == 1.0) {
        pixmap_copy(img_pm, wnd, ctx.img_x, ctx.img_y, img->alpha);
    } else if (ctx.antialiasing) {
        pixmap_scale_bicubic(img_pm, wnd, ctx.img_x, ctx.img_y, ctx.scale,
                             img->alpha);
    } else {
        pixmap_scale_nearest(img_pm, wnd, ctx.img_x, ctx.img_y, ctx.scale,
                             img->alpha);
    }
}

/**
 * Reload image file and reset state (position, scale, etc).
 */
static void reload(void)
{
    const size_t index = fetcher_current()->index;

    if (fetcher_reset(index, false)) {
        if (index == fetcher_current()->index) {
            info_update(info_status, "Image reloaded");
        } else {
            info_update(info_status, "Unable to update, open next file");
        }
        reset_state();
    } else {
        printf("No more images to view, exit\n");
        app_exit(0);
    }
}

/**
 * Redraw handler.
 */
static void redraw(void)
{
    struct pixmap* window = ui_draw_begin();
    if (!window) {
        return;
    }

    draw_image(window);

    // put text info blocks on window surface
    if (!ctx.info_timedout) {
        info_print(window);
    }

    if (ctx.help) {
        text_print_lines(window, text_center, ctx.help, ctx.help_sz);
    }

    // reset one-time rendered notification message
    info_update(info_status, NULL);

    ui_draw_commit();
}

/**
 * Window resize handler.
 */
static void on_resize(void)
{
    fixup_position(false);
    reset_state();
}

/**
 * Key press handler.
 * @param key code of key pressed
 * @param mods key modifiers (ctrl/alt/shift)
 */
static void on_keyboard(xkb_keysym_t key, uint8_t mods)
{
    const struct keybind* kb = keybind_get(key, mods);

    if (!kb) {
        char* name = keybind_name(key, mods);
        if (name) {
            info_update(info_status, "Key %s is not bound", name);
            free(name);
            app_redraw();
        }
        return;
    }

    // handle actions
    for (size_t i = 0; i < kb->num_actions; ++i) {
        const struct action* action = &kb->actions[i];
        switch (action->type) {
            case action_none:
                break;
            case action_help:
                switch_help();
                break;
            case action_first_file:
            case action_last_file:
            case action_prev_dir:
            case action_next_dir:
            case action_prev_file:
            case action_next_file:
                next_image(action->type);
                break;
            case action_skip_file:
                if (!skip_image()) {
                    printf("No more images, exit\n");
                    app_exit(0);
                    return;
                }
                reset_state();
                break;
            case action_prev_frame:
            case action_next_frame:
                animation_ctl(false);
                next_frame(action->type == action_next_frame);
                break;
            case action_animation:
                animation_ctl(!ctx.animation_enable);
                break;
            case action_slideshow:
                slideshow_ctl(!ctx.slideshow_enable &&
                              next_image(action_next_file));
                break;
            case action_fullscreen:
                ui_toggle_fullscreen();
                break;
            case action_mode:
                app_switch_mode(fetcher_current()->index);
                break;
            case action_step_left:
                move_image(true, true, action->params);
                break;
            case action_step_right:
                move_image(true, false, action->params);
                break;
            case action_step_up:
                move_image(false, true, action->params);
                break;
            case action_step_down:
                move_image(false, false, action->params);
                break;
            case action_zoom:
                zoom_image(action->params);
                break;
            case action_rotate_left:
                rotate_image(false);
                break;
            case action_rotate_right:
                rotate_image(true);
                break;
            case action_flip_vertical:
                image_flip_vertical(fetcher_current());
                app_redraw();
                break;
            case action_flip_horizontal:
                image_flip_horizontal(fetcher_current());
                app_redraw();
                break;
            case action_antialiasing:
                ctx.antialiasing = !ctx.antialiasing;
                info_update(info_status, "Anti-aliasing %s",
                            ctx.antialiasing ? "on" : "off");
                app_redraw();
                break;
            case action_reload:
                reload();
                break;
            case action_info:
                info_set_mode(action->params);
                app_redraw();
                break;
            case action_exec:
                app_execute(action->params, fetcher_current()->source);
                break;
            case action_status:
                info_update(info_status, "%s", action->params);
                app_redraw();
                break;
            case action_exit:
                if (ctx.help) {
                    switch_help(); // remove help overlay
                } else {
                    app_exit(0);
                    return;
                }
                break;
        }
        ++action;
    }
}

/**
 * Image drag handler.
 * @param dx,dy delta to move viewpoint
 */
static void on_drag(int dx, int dy)
{
    const ssize_t old_x = ctx.img_x;
    const ssize_t old_y = ctx.img_y;

    ctx.img_x += dx;
    ctx.img_y += dy;

    if (ctx.img_x != old_x || ctx.img_y != old_y) {
        fixup_position(false);
        app_redraw();
    }
}

/**
 * Custom section loader, see `config_loader` for details.
 */
static enum config_status load_config_general(const char* key,
                                              const char* value)
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
            ctx.image_bkg = GRID_BKGID;
            status = cfgst_ok;
        } else if (config_to_color(value, &ctx.image_bkg)) {
            status = cfgst_ok;
        }
    } else if (strcmp(key, VIEWER_CFG_BACKGROUND) == 0) {
        if (config_to_color(value, &ctx.window_bkg)) {
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

/**
 * Custom section loader, see `config_loader` for details.
 */
static enum config_status load_config_imglist(const char* key,
                                              const char* value)
{
    enum config_status status = cfgst_invalid_value;

    if (strcmp(key, IMGLIST_CFG_CACHE) == 0) {
        ssize_t num;
        if (str_to_num(value, 0, &num, 0) && num >= 0 && num < 1024) {
            ctx.history = num;
            status = cfgst_ok;
        }
    } else if (strcmp(key, IMGLIST_CFG_PRELOAD) == 0) {
        ssize_t num;
        if (str_to_num(value, 0, &num, 0) && num >= 0 && num < 1024) {
            ctx.preload = num;
            status = cfgst_ok;
        }
    } else {
        status = cfgst_invalid_key;
    }

    return status;
}

void viewer_create(void)
{
    // set default configuration
    ctx.image_bkg = GRID_BKGID;
    ctx.fixed = true;
    ctx.animation_enable = true;
    ctx.animation_fd = -1;
    ctx.slideshow_enable = false;
    ctx.slideshow_fd = -1;
    ctx.slideshow_time = 3;
    ctx.info_timedout = false;
    ctx.info_timeout_fd = -1;
    ctx.history = 1;
    ctx.preload = 1;

    // register configuration loader
    config_add_loader(GENERAL_CONFIG_SECTION, load_config_general);
    config_add_loader(IMGLIST_CFG_SECTION, load_config_imglist);
}

void viewer_init(struct image* image)
{
    // setup animation timer
    ctx.animation_fd =
        timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (ctx.animation_fd != -1) {
        app_watch(ctx.animation_fd, on_animation_timer);
    }

    // setup slideshow timer
    ctx.slideshow_fd =
        timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (ctx.slideshow_fd != -1) {
        app_watch(ctx.slideshow_fd, on_slideshow_timer);
    }

    // setup info block timer
    if (info_timeout() != 0) {
        ctx.info_timeout_fd =
            timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
        if (ctx.info_timeout_fd != -1) {
            app_watch(ctx.info_timeout_fd, on_info_block_timeout);
        }
    }

    fetcher_init(image, ctx.history, ctx.preload);
}

void viewer_destroy(void)
{
    fetcher_destroy();

    for (size_t i = 0; i < ctx.help_sz; i++) {
        free(ctx.help[i].data);
    }
    if (ctx.animation_fd != -1) {
        close(ctx.animation_fd);
    }
    if (ctx.slideshow_fd != -1) {
        close(ctx.slideshow_fd);
    }
    if (ctx.info_timeout_fd != -1) {
        close(ctx.info_timeout_fd);
    }
}

void viewer_handle(const struct event* event)
{
    switch (event->type) {
        case event_reload:
            reload();
            break;
        case event_redraw:
            redraw();
            break;
        case event_resize:
            on_resize();
            break;
        case event_keypress:
            on_keyboard(event->param.keypress.key, event->param.keypress.mods);
            break;
        case event_drag:
            on_drag(event->param.drag.dx, event->param.drag.dy);
            break;
        case event_activate:
            if (fetcher_reset(event->param.activate.index, false)) {
                reset_state();
            } else {
                app_exit(0);
            }
            break;
    }
}
