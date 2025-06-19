// SPDX-License-Identifier: MIT
// Image viewer mode.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "viewer.h"

#include "application.h"
#include "array.h"
#include "buildcfg.h"
#include "cache.h"
#include "imglist.h"
#include "info.h"
#include "render.h"
#include "ui.h"

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timerfd.h>
#include <unistd.h>

// Window background modes
#define BKGMODE_ID_AUTO     0x00f1f2f3
#define BKGMODE_NAME_AUTO   "auto"
#define BKGMODE_ID_EXTEND   0x00f1f2f4
#define BKGMODE_NAME_EXTEND "extend"
#define BKGMODE_ID_MIRROR   0x00f1f2f5
#define BKGMODE_NAME_MIRROR "mirror"

// Background grid parameters
#define GRID_NAME   "grid"
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
    scale_keep_zoom,   ///< Keep absolute zoom across images
};

// clang-format off
static const char* scale_names[] = {
    [scale_fit_optimal] = "optimal",
    [scale_fit_window] = "fit",
    [scale_fit_width] = "width",
    [scale_fit_height] = "height",
    [scale_fill_window] = "fill",
    [scale_real_size] = "real",
    [scale_keep_zoom] = "keep",
};
// clang-format on

/** Image position. */
enum position {
    position_free,
    position_center,
    position_top,
    position_bottom,
    position_left,
    position_right,
    position_tl,
    position_tr,
    position_bl,
    position_br,
};

// clang-format off
static const char* position_names[] = {
    [position_free] = "free",
    [position_center] = "center",
    [position_top] = "top",
    [position_bottom] = "bottom",
    [position_left] = "left",
    [position_right] = "right",
    [position_tl] = "top_left",
    [position_tr] = "top_right",
    [position_bl] = "bottom_left",
    [position_br] = "bottom_right",
};
// clang-format on

/** Viewer context. */
struct viewer {
    struct image* current; ///< Currently shown image

    struct cache* history; ///< Recently viewed images
    struct cache* preload; ///< Preloaded images
    pthread_t preload_tid; ///< Preload thread id
    bool preload_active;   ///< Preload in progress flag

    ssize_t img_x, img_y; ///< Top left corner of the image
    ssize_t img_w, img_h; ///< Image width and height

    size_t frame;         ///< Index of the current frame
    argb_t image_bkg;     ///< Image background mode/color
    argb_t window_bkg;    ///< Window background mode/color
    enum aa_mode aa_mode; ///< Anti-aliasing mode

    enum fixed_scale scale_init; ///< Initial scale
    enum position position;      ///< Initial position
    double scale;                ///< Current scale factor of the image

    bool animation_enable; ///< Animation enable/disable
    int animation_fd;      ///< Animation timer

    bool slideshow_enable; ///< Slideshow enable/disable
    int slideshow_fd;      ///< Slideshow timer
    size_t slideshow_time; ///< Slideshow image display time (seconds)

    struct keybind* kb; ///< Key bindings
};

/** Global viewer context. */
static struct viewer ctx;

/**
 * Preloader thread.
 */
static void* preloader_thread(__attribute__((unused)) void* data)
{
    struct image* current = ctx.current;
    size_t counter = 0;

    while (ctx.preload_active && counter < cache_capacity(ctx.preload)) {
        struct image* next;
        struct image* origin;
        enum image_status status;

        imglist_lock();

        if (current != ctx.current) {
            current = ctx.current;
            counter = 0;
        }

        // get next image
        next = imglist_jump(ctx.current, counter + 1);
        if (!next) {
            // last image in the list
            imglist_unlock();
            cache_trim(ctx.preload, counter);
            break;
        }

        // get existing image form history/preload cache
        if (cache_out(ctx.preload, next) || cache_out(ctx.history, next)) {
            cache_put(ctx.preload, next);
            imglist_unlock();
            ++counter;
            continue;
        }

        // create copy to unlock the list
        next = image_create(next->source);
        imglist_unlock();
        if (!next) {
            break; // not enough memory
        }

        // load next image
        status = image_load(next);

        imglist_lock();

        origin = imglist_find(next->source);
        if (!origin || image_has_frames(origin)) {
            image_free(next, IMGDATA_SELF);
            imglist_unlock();
            continue; // already skipped or loaded by main thread
        }

        if (status != imgload_success) {
            imglist_remove(origin);
        } else {
            // replace existing image data
            image_attach(origin, next);
            if (!cache_put(ctx.preload, origin)) {
                // not enough memory
                image_free(origin, IMGDATA_FRAMES);
                image_free(next, IMGDATA_SELF);
                break;
            }
            ++counter;
        }

        image_free(next, IMGDATA_SELF);
        imglist_unlock();
    }

    ctx.preload_active = false;
    return NULL;
}

/**
 * Start preloader.
 */
static void preloader_start(void)
{
    if (ctx.preload && !ctx.preload_active) {
        ctx.preload_active = true;
        pthread_create(&ctx.preload_tid, NULL, preloader_thread, NULL);
    }
}

/**
 * Stop preloader.
 */
static void preloader_stop(void)
{
    if (ctx.preload_active) {
        ctx.preload_active = false;
        pthread_join(ctx.preload_tid, NULL);
    }
}

/**
 * Fix up image position.
 * @param force flag to force update position
 */
static void fixup_position(bool force)
{
    const ssize_t wnd_width = ui_get_width();
    const ssize_t wnd_height = ui_get_height();

    const struct imgframe* frame =
        arr_nth(ctx.current->data->frames, ctx.frame);
    const ssize_t img_width = ctx.scale * frame->pm.width;
    const ssize_t img_height = ctx.scale * frame->pm.height;

    if (force || (img_width <= wnd_width && ctx.position != position_free)) {
        switch (ctx.position) {
            case position_free:
                ctx.img_x = wnd_width / 2 - img_width / 2;
                break;
            case position_top:
                ctx.img_x = wnd_width / 2 - img_width / 2;
                break;
            case position_center:
                ctx.img_x = wnd_width / 2 - img_width / 2;
                break;
            case position_bottom:
                ctx.img_x = wnd_width / 2 - img_width / 2;
                break;
            case position_left:
                ctx.img_x = 0;
                break;
            case position_right:
                ctx.img_x = wnd_width - img_width;
                break;
            case position_tl:
                ctx.img_x = 0;
                break;
            case position_tr:
                ctx.img_x = wnd_width - img_width;
                break;
            case position_bl:
                ctx.img_x = 0;
                break;
            case position_br:
                ctx.img_x = wnd_width - img_width;
                break;
        }
    }
    if (force || (img_height <= wnd_height && ctx.position != position_free)) {
        switch (ctx.position) {
            case position_free:
                ctx.img_y = wnd_height / 2 - img_height / 2;
                break;
            case position_top:
                ctx.img_y = 0;
                break;
            case position_center:
                ctx.img_y = wnd_height / 2 - img_height / 2;
                break;
            case position_bottom:
                ctx.img_y = wnd_height - img_height;
                break;
            case position_left:
                ctx.img_y = wnd_height / 2 - img_height / 2;
                break;
            case position_right:
                ctx.img_y = wnd_height / 2 - img_height / 2;
                break;
            case position_tl:
                ctx.img_y = 0;
                break;
            case position_tr:
                ctx.img_y = 0;
                break;
            case position_bl:
                ctx.img_y = wnd_height - img_height;
                break;
            case position_br:
                ctx.img_y = wnd_height - img_height;
                break;
        }
    }

    if (ctx.position != position_free) {
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
    const struct imgframe* frame =
        arr_nth(ctx.current->data->frames, ctx.frame);
    const ssize_t diff = (ssize_t)frame->pm.width - frame->pm.height;
    const ssize_t shift = (ctx.scale * diff) / 2;

    image_rotate(ctx.current, clockwise ? 90 : 270);
    ctx.img_x += shift;
    ctx.img_y -= shift;
    fixup_position(false);

    app_redraw();
}

/**
 * Set absolute scale.
 * @param scale scale to set
 */
static void set_absolute_scale(double scale)
{
    // save center
    const double wnd_half_w = (double)ui_get_width() / 2;
    const double wnd_half_h = (double)ui_get_height() / 2;
    const double center_x = wnd_half_w / ctx.scale - ctx.img_x / ctx.scale;
    const double center_y = wnd_half_h / ctx.scale - ctx.img_y / ctx.scale;

    // check scale limits
    if (scale > MAX_SCALE) {
        scale = MAX_SCALE;
    } else {
        const struct imgframe* frame =
            arr_nth(ctx.current->data->frames, ctx.frame);
        const double scale_w = (double)MIN_SCALE / frame->pm.width;
        const double scale_h = (double)MIN_SCALE / frame->pm.height;
        const double scale_min = max(scale_w, scale_h);
        if (scale < scale_min) {
            scale = scale_min;
        }
    }

    ctx.scale = scale;

    // restore center
    ctx.img_x = wnd_half_w - center_x * ctx.scale;
    ctx.img_y = wnd_half_h - center_y * ctx.scale;

    fixup_position(false);

    info_update(info_scale, "%.0f%%", ctx.scale * 100);
    app_redraw();
}

/**
 * Set scale from one of the fixed modes.
 * @param mode fixed scale mode to set
 */
static void set_fixed_scale(enum fixed_scale mode)
{
    const struct imgframe* frame =
        arr_nth(ctx.current->data->frames, ctx.frame);
    const double wnd_width = ui_get_width();
    const double wnd_height = ui_get_height();
    const double scale_w = wnd_width / frame->pm.width;
    const double scale_h = wnd_height / frame->pm.height;
    double scale = 1.0;

    switch (mode) {
        case scale_fit_optimal:
            scale = min(scale_w, scale_h);
            if (scale > 1.0) {
                scale = 1.0;
            }
            break;
        case scale_fit_window:
            scale = min(scale_w, scale_h);
            break;
        case scale_fit_width:
            scale = scale_w;
            break;
        case scale_fit_height:
            scale = scale_h;
            break;
        case scale_fill_window:
            scale = max(scale_w, scale_h);
            break;
        case scale_real_size:
            scale = 1.0; // 100 %
            break;
        case scale_keep_zoom:
            app_redraw();
            return;
    }

    set_absolute_scale(scale);
    fixup_position(true); // force reposition even in free moving mode
}

/**
 * Zoom image: handle "zoom" action.
 * @param params zoom action parameter
 */
static void zoom_image(const char* params)
{
    ssize_t scale_mode;

    if (!params || !*params) {
        // switch to the next scale mode
        const size_t index = ctx.scale_init + 1 < ARRAY_SIZE(scale_names)
            ? ctx.scale_init + 1
            : 0;
        zoom_image(scale_names[index]);
        return;
    }

    // one of the fixed modes
    scale_mode = str_index(scale_names, params, 0);
    if (scale_mode >= 0) {
        ctx.scale_init = scale_mode;
        set_fixed_scale(scale_mode);
        info_update(info_status, "Scale mode: %s", params);
    } else {
        double scale = 0;

        if (params[0] == '+' || params[0] == '-') {
            // relative
            ssize_t delta;
            if (str_to_num(params, 0, &delta, 0) && delta != 0 &&
                delta > -1000 && delta < 1000) {
                scale = ctx.scale + (ctx.scale / 100) * delta;
            }
        } else {
            // percent
            ssize_t percent;
            if (str_to_num(params, 0, &percent, 0) && percent > 0 &&
                percent < MAX_SCALE * 100) {
                scale = (double)percent / 100;
            }
        }

        if (scale != 0) {
            set_absolute_scale(scale);
        } else {
            info_update(info_status, "Invalid zoom operation: %s", params);
            app_redraw();
        }
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
        const struct imgframe* frame =
            arr_nth(ctx.current->data->frames, ctx.frame);
        const size_t duration = frame->duration;
        enable = (ctx.current->data->frames->size > 1 && duration);
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
    const struct imgframe* frame = arr_nth(ctx.current->data->frames, 0);
    ctx.frame = 0;

    if (ctx.scale_init != scale_keep_zoom) {
        set_fixed_scale(ctx.scale_init);
    } else {
        if (ctx.scale == 0) {
            set_fixed_scale(scale_fit_optimal);
        } else {
            const ssize_t diff_w = ctx.img_w - frame->pm.width;
            const ssize_t diff_h = ctx.img_h - frame->pm.height;
            ctx.img_x += floor(ctx.scale * diff_w) / 2.0;
            ctx.img_y += floor(ctx.scale * diff_h) / 2.0;
            fixup_position(false);
        }
    }

    ctx.img_w = frame->pm.width;
    ctx.img_h = frame->pm.height;

    ui_set_title(ctx.current->name);
    animation_ctl(true);
    slideshow_ctl(ctx.slideshow_enable);

    info_reset(ctx.current);
    info_update_index(ctx.current->index, imglist_size());
    info_update(info_scale, "%.0f%%", ctx.scale * 100);

    ui_set_ctype(ctx.animation_enable);

    app_redraw();
}

/**
 * Load image and set it as the current.
 * @param img image to open
 * @param forward preferred direction after skipping file
 * @return pointer to `ctx.current` or NULL if image list is empty
 */
static struct image* open_image(struct image* img, bool forward)
{
    while (img) {
        struct image* next;

        // get file form history/preload cache
        if (cache_out(ctx.preload, img) || cache_out(ctx.history, img)) {
            break;
        }
        if (image_load(img) == imgload_success) {
            break;
        }

        // skip and jump to the nearest entry
        next = forward ? imglist_next_file(img) : imglist_prev_file(img);
        if (next == ctx.current) {
            next = NULL;
        } else {
            imglist_remove(img);
        }
        img = next;
    }

    if (img) {
        if (!cache_put(ctx.history, ctx.current)) {
            image_free(ctx.current, IMGDATA_FRAMES);
        }
        ctx.current = img;
        preloader_start();
        reset_state();
    } else {
        info_update_index(ctx.current->index, imglist_size());
        app_redraw();
    }

    return img;
}

/**
 * Switch to the next image.
 * @param direction next image position
 * @return true if next image was loaded
 */
static bool next_image(enum action_type direction)
{
    struct image* next;
    bool forward = false; // preferred direction after skipping file

    imglist_lock();

    switch (direction) {
        case action_first_file:
            next = imglist_first();
            forward = true;
            break;
        case action_last_file:
            next = imglist_last();
            break;
        case action_prev_dir:
            next = imglist_prev_dir(ctx.current);
            break;
        case action_next_dir:
            next = imglist_next_dir(ctx.current);
            forward = true;
            break;
        case action_prev_file:
            next = imglist_prev_file(ctx.current);
            break;
        case action_next_file:
            next = imglist_next_file(ctx.current);
            forward = true;
            break;
        case action_rand_file:
            next = imglist_rand(ctx.current);
            forward = true;
            break;
        default:
            next = NULL;
            break;
    }

    next = open_image(next, forward);

    imglist_unlock();

    return next;
}

/**
 * Switch to the next or previous frame.
 * @param forward switch direction
 */
static void next_frame(bool forward)
{
    size_t index = ctx.frame;

    if (forward) {
        if (++index >= ctx.current->data->frames->size) {
            index = 0;
        }
    } else {
        if (index-- == 0) {
            index = ctx.current->data->frames->size - 1;
        }
    }

    if (index != ctx.frame) {
        const struct imgframe* frame =
            arr_nth(ctx.current->data->frames, ctx.frame);
        ctx.frame = index;
        info_update(info_frame, "%zu of %zu", ctx.frame + 1,
                    ctx.current->data->frames->size);
        info_update(info_image_size, "%zux%zu", frame->pm.width,
                    frame->pm.height);
        app_redraw();
    }
}

/** Animation timer event handler. */
static void on_animation_timer(__attribute__((unused)) void* data)
{
    next_frame(true);
    animation_ctl(true);
}

/** Slideshow timer event handler. */
static void on_slideshow_timer(__attribute__((unused)) void* data)
{
    slideshow_ctl(next_image(action_next_file));
}

/**
 * Skip current image file.
 * @param remove flag to remove current image from the image list
 * @return true if next image opened
 */
static bool skip_current(bool remove)
{
    struct image* curr = ctx.current;
    struct image* next;

    next = imglist_next_file(ctx.current);
    next = open_image(next, true);
    if (!next) {
        next = imglist_prev_file(ctx.current);
        next = open_image(next, false);
    }

    if (!next) {
        fprintf(stderr, "No more images to view, exit\n");
        app_exit(0);
    } else if (remove) {
        imglist_remove(curr);
    }

    return next;
}

/**
 * Reload image file and reset state (position, scale, etc).
 */
static void reload_current(void)
{
    if (image_load(ctx.current) == imgload_success) {
        info_update(info_status, "Image reloaded");
        reset_state();
    } else {
        info_update(info_status, "Unable to reload file, open next one");
        skip_current(true);
    }
}

/**
 * Draw image.
 * @param wnd pixel map of target window
 */
static void draw_image(struct pixmap* wnd)
{
    const struct imgframe* frame =
        arr_nth(ctx.current->data->frames, ctx.frame);
    const struct pixmap* pm = &frame->pm;
    const size_t width = ctx.scale * pm->width;
    const size_t height = ctx.scale * pm->height;

    // clear image background
    if (frame->pm.format == pixmap_argb) {
        if (ctx.image_bkg == GRID_BKGID) {
            pixmap_grid(wnd, ctx.img_x, ctx.img_y, width, height, GRID_STEP,
                        GRID_COLOR1, GRID_COLOR2);
        } else {
            pixmap_fill(wnd, ctx.img_x, ctx.img_y, width, height,
                        ctx.image_bkg);
        }
    }

    // put image on window surface
    image_render(ctx.current, ctx.frame, ctx.aa_mode, ctx.scale, true,
                 ctx.img_x, ctx.img_y, wnd);

    // set window background
    switch (ctx.window_bkg) {
        case BKGMODE_ID_AUTO:
            if (width > height) {
                pixmap_bkg_mirror(wnd, ctx.img_x, ctx.img_y, width, height);
            } else {
                pixmap_bkg_extend(wnd, ctx.img_x, ctx.img_y, width, height);
            }
            break;
        case BKGMODE_ID_EXTEND:
            pixmap_bkg_extend(wnd, ctx.img_x, ctx.img_y, width, height);
            break;
        case BKGMODE_ID_MIRROR:
            pixmap_bkg_mirror(wnd, ctx.img_x, ctx.img_y, width, height);
            break;
        default:
            pixmap_inverse_fill(wnd, ctx.img_x, ctx.img_y, width, height,
                                ctx.window_bkg);
    }
}

/** Redraw window. */
static void redraw(void)
{
    struct pixmap* wnd = ui_draw_begin();
    if (wnd) {
        draw_image(wnd);
        info_print(wnd);
        ui_draw_commit();
    }
}

/** Mode handler: window resize. */
static void on_resize(void)
{
    fixup_position(false);
    reset_state();
}

/** Mode handler: image list update. */
static void on_imglist(const struct image* image, enum fsevent event)
{
    switch (event) {
        case fsevent_create:
            break;
        case fsevent_modify:
            if (image == ctx.current) {
                reload_current();
            } else {
                cache_out(ctx.preload, image);
                cache_out(ctx.history, image);
            }
            break;
        case fsevent_remove:
            if (image == ctx.current) {
                skip_current(false);
            } else {
                cache_out(ctx.preload, image);
                cache_out(ctx.history, image);
            }
            break;
    }
}

/** Mode handler: mouse move. */
static void on_mouse_move(uint8_t mods, uint32_t btn,
                          __attribute__((unused)) size_t x,
                          __attribute__((unused)) size_t y, ssize_t dx,
                          ssize_t dy)
{
    const struct keybind* kb = keybind_find(ctx.kb, MOUSE_TO_XKB(btn), mods);
    if (kb && kb->actions->type == action_drag) {
        ctx.img_x += dx;
        ctx.img_y += dy;
        fixup_position(false);
        app_redraw();
    }
}

/** Mode handler: mouse click/scroll. */
static bool on_mouse_click(uint8_t mods, uint32_t btn,
                           __attribute__((unused)) size_t x,
                           __attribute__((unused)) size_t y)
{
    const struct keybind* kb = keybind_find(ctx.kb, MOUSE_TO_XKB(btn), mods);
    if (kb && kb->actions->type == action_drag) {
        ui_set_cursor(ui_cursor_drag);
        return true;
    }
    return false;
}

/** Mode handler: apply action. */
static void handle_action(const struct action* action)
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
            imglist_lock();
            skip_current(true);
            imglist_unlock();
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
            image_flip_vertical(ctx.current);
            app_redraw();
            break;
        case action_flip_horizontal:
            image_flip_horizontal(ctx.current);
            app_redraw();
            break;
        case action_antialiasing:
            ctx.aa_mode = aa_switch(ctx.aa_mode, action->params);
            info_update(info_status, "Anti-aliasing: %s", aa_name(ctx.aa_mode));
            app_redraw();
            break;
        case action_redraw:
            redraw();
            break;
        case action_reload:
            imglist_lock();
            reload_current();
            imglist_unlock();
            break;
        case action_export:
            if (!*action->params) {
                info_update(info_status, "Error: export path is not specified");
            } else if (image_export(ctx.current, ctx.frame, action->params)) {
                info_update(info_status, "Exported to %s", action->params);
            } else {
                info_update(info_status, "Error: export failed");
            }
            app_redraw();
            break;
        default:
            break;
    }
}

/** Mode handler: get currently viewed image. */
static struct image* get_current(void)
{
    return ctx.current;
}

/** Mode handler: activate viewer. */
static void on_activate(struct image* image)
{
    ctx.current = image;
    cache_out(ctx.preload, ctx.current);
    cache_out(ctx.history, ctx.current);

    if (image_has_frames(ctx.current) ||
        image_load(ctx.current) == imgload_success || skip_current(true)) {
        reset_state();
        preloader_start();
    }
}

/** Mode handler: deactivate viewer. */
static void on_deactivate(void)
{
    preloader_stop();
    animation_ctl(false);
    slideshow_ctl(false);
    cache_put(ctx.history, ctx.current);
}

/** Mode handler: get key bindings. */
static struct keybind* get_keybinds(void)
{
    return ctx.kb;
}

void viewer_init(const struct config* cfg, struct mode* handlers)
{
    const struct config* section = config_section(cfg, CFG_VIEWER);
    size_t cval_num;
    const char* cval_txt;

    ctx.aa_mode = aa_init(section, CFG_VIEW_AA);

    // window background
    cval_txt = config_get(section, CFG_VIEW_WINDOW);
    if (strcmp(cval_txt, BKGMODE_NAME_AUTO) == 0) {
        ctx.window_bkg = BKGMODE_ID_AUTO;
    } else if (strcmp(cval_txt, BKGMODE_NAME_EXTEND) == 0) {
        ctx.window_bkg = BKGMODE_ID_EXTEND;
    } else if (strcmp(cval_txt, BKGMODE_NAME_MIRROR) == 0) {
        ctx.window_bkg = BKGMODE_ID_MIRROR;
    } else {
        ctx.window_bkg = config_get_color(section, CFG_VIEW_WINDOW);
    }

    // background for transparent images
    cval_txt = config_get(section, CFG_VIEW_TRANSP);
    if (strcmp(cval_txt, GRID_NAME) == 0) {
        ctx.image_bkg = GRID_BKGID;
    } else {
        ctx.image_bkg = config_get_color(section, CFG_VIEW_TRANSP);
    }

    // initial scale and position
    ctx.scale_init = config_get_oneof(section, CFG_VIEW_SCALE, scale_names,
                                      ARRAY_SIZE(scale_names));
    ctx.position = config_get_oneof(section, CFG_VIEW_POSITION, position_names,
                                    ARRAY_SIZE(position_names));

    // history and preloads caches
    cval_num = config_get_num(section, CFG_VIEW_HISTORY, 0, 1024);
    ctx.history = cache_init(cval_num);
    cval_num = config_get_num(section, CFG_VIEW_PRELOAD, 0, 1024);
    ctx.preload = cache_init(cval_num);

    // setup animation timer
    ctx.animation_enable = true;
    ctx.animation_fd =
        timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (ctx.animation_fd != -1) {
        app_watch(ctx.animation_fd, on_animation_timer, NULL);
    }
    // setup slideshow timer
    ctx.slideshow_enable = config_get_bool(section, CFG_VIEW_SSHOW);
    ctx.slideshow_time = config_get_num(section, CFG_VIEW_SSHOW_TM, 1, 86400);
    ctx.slideshow_fd =
        timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (ctx.slideshow_fd != -1) {
        app_watch(ctx.slideshow_fd, on_slideshow_timer, NULL);
    }

    // load key bindings
    ctx.kb = keybind_load(config_section(cfg, CFG_KEYS_VIEWER));

    handlers->on_activate = on_activate;
    handlers->on_deactivate = on_deactivate;
    handlers->on_resize = on_resize;
    handlers->on_mouse_move = on_mouse_move;
    handlers->on_mouse_click = on_mouse_click;
    handlers->on_imglist = on_imglist;
    handlers->handle_action = handle_action;
    handlers->get_current = get_current;
    handlers->get_keybinds = get_keybinds;
}

void viewer_destroy(void)
{
    if (ctx.animation_fd != -1) {
        close(ctx.animation_fd);
    }
    if (ctx.slideshow_fd != -1) {
        close(ctx.slideshow_fd);
    }
    keybind_free(ctx.kb);

    cache_free(ctx.history);
    cache_free(ctx.preload);
}
