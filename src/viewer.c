// SPDX-License-Identifier: MIT
// Business logic of application and UI event handlers.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "viewer.h"

#include "application.h"
#include "buildcfg.h"
#include "fetcher.h"
#include "imagelist.h"
#include "info.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>
#include <sys/timerfd.h>
#include <unistd.h>

// Background grid parameters
#define GRID_NAME   "grid"
#define GRID_BKGID  0x00f1f2f3
#define GRID_STEP   10
#define GRID_COLOR1 0xff333333
#define GRID_COLOR2 0xff4c4c4c

// Default configuration parameters
#define CFG_WINDOW_DEF         ARGB(0, 0, 0, 0)
#define CFG_TRANSPARENCY_DEF   GRID_NAME
#define CFG_SCALE_DEF          "optimal"
#define CFG_POSITION_DEF       "center"
#define CFG_FIXED_DEF          true
#define CFG_ANTIALIASING_DEF   false
#define CFG_SLIDESHOW_DEF      false
#define CFG_SLIDESHOW_TIME_DEF 3
#define CFG_HISTORY_DEF        1
#define CFG_PRELOAD_DEF        1

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

enum position {
    position_top,
    position_center,
    position_bottom,
    position_left,
    position_right,
    position_top_left,
    position_top_right,
    position_bottom_left,
    position_bottom_right,
};

// clang-format off
static const char* position_names[] = {
    [position_top] = "top",
    [position_center] = "center",
    [position_bottom] = "bottom",
    [position_left] = "left",
    [position_right] = "right",
    [position_top_left] = "topleft",
    [position_top_right] = "topright",
    [position_bottom_left] = "bottomleft",
    [position_bottom_right] = "bottomright",
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
    enum position position;      ///< Initial position
    float scale;                 ///< Current scale factor of the image

    bool animation_enable; ///< Animation enable/disable
    int animation_fd;      ///< Animation timer

    bool slideshow_enable; ///< Slideshow enable/disable
    int slideshow_fd;      ///< Slideshow timer
    size_t slideshow_time; ///< Slideshow image display time (seconds)
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

    switch (ctx.position) {
        case position_top:
            ctx.img_y = 0;
            ctx.img_x = wnd_width / 2 - (ctx.scale * pm->width) / 2;
            break;
        case position_center:
            ctx.img_y = wnd_height / 2 - (ctx.scale * pm->height) / 2;
            ctx.img_x = wnd_width / 2 - (ctx.scale * pm->width) / 2;
            break;
        case position_bottom:
            ctx.img_y = wnd_height - ctx.scale * pm->height;
            ctx.img_x = wnd_width / 2 - (ctx.scale * pm->width) / 2;
            break;
        case position_left:
            ctx.img_y = wnd_height / 2 - (ctx.scale * pm->height) / 2;
            ctx.img_x = 0;
            break;
        case position_right:
            ctx.img_y = wnd_height / 2 - (ctx.scale * pm->height) / 2;
            ctx.img_x = wnd_width - ctx.scale * pm->width;
            break;
        case position_top_left:
            ctx.img_y = 0;
            ctx.img_x = 0;
            break;
        case position_top_right:
            ctx.img_y = 0;
            ctx.img_x = wnd_width - ctx.scale * pm->width;
            break;
        case position_bottom_left:
            ctx.img_y = wnd_height - ctx.scale * pm->height;
            ctx.img_x = 0;
            break;
        case position_bottom_right:
            ctx.img_y = wnd_height - ctx.scale * pm->height;
            ctx.img_x = wnd_width - ctx.scale * pm->width;
            break;
    }

    fixup_position(true);
    info_update(info_scale, "%.0f%%", ctx.scale * 100);
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
        const double wnd_half_w = (double)ui_get_width() / 2;
        const double wnd_half_h = (double)ui_get_height() / 2;
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
 * Set default scale to a fixed scale value and apply it.
 * @param params fixed scale to set
 */
static void scale_global(const char* params)
{
    if (params && *params) {
        ssize_t fixed_scale = str_index(scale_names, params, 0);

        if (fixed_scale >= 0) {
            ctx.scale_init = fixed_scale;
        } else {
            fprintf(stderr, "Invalid scale operation: \"%s\"\n", params);
            return;
        }
    } else {
        // toggle to the next scale
        ctx.scale_init++;
        if (ctx.scale_init >= ARRAY_SIZE(scale_names)) {
            ctx.scale_init = 0;
        }
    }

    info_update(info_status, "Scale %s", scale_names[ctx.scale_init]);
    scale_image(ctx.scale_init);
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
    const size_t total_img = image_list_size();

    ctx.frame = 0;
    ctx.img_x = 0;
    ctx.img_y = 0;
    ctx.scale = 0;
    scale_image(ctx.scale_init);

    ui_set_title(img->name);
    animation_ctl(true);
    slideshow_ctl(ctx.slideshow_enable);

    info_reset(img);
    if (total_img) {
        info_update(info_index, "%zu of %zu", img->index + 1, total_img);
    }

    ui_set_content_type_animated(ctx.animation_enable);

    app_redraw();
}

/**
 * Skip current image.
 * @return true if next image was loaded
 */
static bool skip_image(void)
{
    size_t index;
    const size_t current = fetcher_current()->index;

    index = image_list_skip(current);
    while (index != IMGLIST_INVALID && !fetcher_open(index)) {
        index = image_list_skip(index);
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
                // look forward in case the first file fails to load
                direction = action_next_file;
                break;
            case action_last_file:
                index = image_list_last();
                // look backward in case the last file fails to load
                direction = action_prev_file;
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
            case action_rand_file:
                index = image_list_rand_file(index);
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
static void on_animation_timer(__attribute__((unused)) void* data)
{
    next_frame(true);
    animation_ctl(true);
}

/**
 * Slideshow timer event handler.
 */
static void on_slideshow_timer(__attribute__((unused)) void* data)
{
    slideshow_ctl(next_image(action_next_file));
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
    } else {
        enum pixmap_scale scaler;
        if (ctx.antialiasing) {
            scaler = (ctx.scale > 1.0) ? pixmap_bicubic : pixmap_average;
        } else {
            scaler = pixmap_nearest;
        }
        pixmap_scale(scaler, img_pm, wnd, ctx.img_x, ctx.img_y, ctx.scale,
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
    if (window) {
        draw_image(window);
        info_print(window);
        ui_draw_commit();
    }
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
 * Apply action.
 * @param action pointer to the action being performed
 */
static void apply_action(const struct action* action)
{
    switch (action->type) {
        case action_first_file:
        case action_last_file:
        case action_prev_dir:
        case action_next_dir:
        case action_prev_file:
        case action_next_file:
        case action_rand_file:
            next_image(action->type);
            break;
        case action_skip_file:
            if (skip_image()) {
                reset_state();
            } else {
                printf("No more images, exit\n");
                app_exit(0);
            }
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
        case action_scale:
            scale_global(action->params);
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
        case action_exec:
            app_execute(action->params, fetcher_current()->source);
            break;
        default:
            break;
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

void viewer_init(struct config* cfg, struct image* image)
{
    size_t history;
    size_t preload;
    const char* value;
    ssize_t index;

    ctx.fixed =
        config_get_bool(cfg, VIEWER_SECTION, VIEWER_FIXED, CFG_FIXED_DEF);
    ctx.antialiasing = config_get_bool(cfg, VIEWER_SECTION, VIEWER_ANTIALIASING,
                                       CFG_ANTIALIASING_DEF);
    ctx.window_bkg =
        config_get_color(cfg, VIEWER_SECTION, VIEWER_WINDOW, CFG_WINDOW_DEF);

    // background for transparent images
    value = config_get_string(cfg, VIEWER_SECTION, VIEWER_TRANSPARENCY,
                              CFG_TRANSPARENCY_DEF);
    if (strcmp(value, GRID_NAME) == 0) {
        ctx.image_bkg = GRID_BKGID;
    } else {
        ctx.image_bkg = config_get_color(cfg, VIEWER_SECTION,
                                         VIEWER_TRANSPARENCY, GRID_BKGID);
    }

    // initial scale
    value = config_get_string(cfg, VIEWER_SECTION, VIEWER_SCALE, CFG_SCALE_DEF);
    index = str_index(scale_names, value, 0);
    if (index >= 0) {
        ctx.scale_init = index;
    } else {
        ctx.scale_init = scale_fit_optimal;
        config_error_val(VIEWER_SECTION, value);
    }

    value = config_get_string(cfg, VIEWER_SECTION, VIEWER_POSITION,
                              CFG_POSITION_DEF);
    index = str_index(position_names, value, 0);
    if (index >= 0) {
        ctx.position = index;
    } else {
        ctx.position = position_center;
    }

    // cache and preloads
    history = config_get_num(cfg, VIEWER_SECTION, VIEWER_HISTORY, 0, 1024,
                             CFG_HISTORY_DEF);
    preload = config_get_num(cfg, VIEWER_SECTION, VIEWER_PRELOAD, 0, 1024,
                             CFG_PRELOAD_DEF);

    // setup animation timer
    ctx.animation_enable = true;
    ctx.animation_fd =
        timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (ctx.animation_fd != -1) {
        app_watch(ctx.animation_fd, on_animation_timer, NULL);
    }

    // setup slideshow timer
    ctx.slideshow_enable = config_get_bool(cfg, VIEWER_SECTION,
                                           VIEWER_SLIDESHOW, CFG_SLIDESHOW_DEF);
    ctx.slideshow_time =
        config_get_num(cfg, VIEWER_SECTION, VIEWER_SLIDESHOW_TIME, 1, 86400,
                       CFG_SLIDESHOW_TIME_DEF);
    ctx.slideshow_fd =
        timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (ctx.slideshow_fd != -1) {
        app_watch(ctx.slideshow_fd, on_slideshow_timer, NULL);
    }

    fetcher_init(image, history, preload);
}

void viewer_destroy(void)
{
    fetcher_destroy();

    if (ctx.animation_fd != -1) {
        close(ctx.animation_fd);
    }
    if (ctx.slideshow_fd != -1) {
        close(ctx.slideshow_fd);
    }
}

void viewer_handle(const struct event* event)
{
    switch (event->type) {
        case event_action:
            apply_action(event->param.action);
            break;
        case event_redraw:
            redraw();
            break;
        case event_resize:
            on_resize();
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
        case event_load:
            fetcher_attach(event->param.load.image, event->param.load.index);
            break;
    }
}
