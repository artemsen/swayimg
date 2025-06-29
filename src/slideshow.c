// SPDX-License-Identifier: MIT
// Slide show mode.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "slideshow.h"

#include "application.h"
#include "imglist.h"
#include "info.h"
#include "render.h"
#include "tpool.h"
#include "ui.h"
#include "viewport.h"

#include <assert.h>
#include <sys/timerfd.h>
#include <unistd.h>

/** Slideshow context. */
struct slideshow {
    struct viewport vp; ///< Viewport
    struct keybind* kb; ///< Key bindings

    struct image* next; ///< Next image to show (preloading)

    int timer_fd;    ///< Timer file
    size_t duration; ///< Slideshow image display time (seconds)
    bool enabled;    ///< Slideshow state (in progress/paused)
};

/** Global slideshow context. */
static struct slideshow ctx;

/**
 * Time control.
 * @param restart true to restart timer, false to stop
 */
static inline void timer_ctl(bool restart)
{
    struct itimerspec ts = { .it_value.tv_sec = restart ? ctx.duration : 0 };
    timerfd_settime(ctx.timer_fd, 0, &ts, NULL);
}

/** Image preloader worker. */
static void preloader(__attribute__((unused)) void* data)
{
    struct image* curr;
    struct image* img;

    imglist_lock();
    curr = ctx.vp.image;

    img = imglist_next(curr, true);
    while (img) {
        struct image* skip;
        if (image_has_frames(img) || image_load(img) == imgload_success) {
            break;
        }
        skip = img;
        img = imglist_next(img, true);
        imglist_remove(skip);
        if (img == curr) {
            img = NULL; // no more images
        }
    }

    ctx.next = img;
    imglist_unlock();
}

/**
 * Start preloader to get next image.
 */
static void start_preloader(void)
{
    assert(imglist_is_locked());

    if (ctx.next && ctx.next != ctx.vp.image) {
        image_free(ctx.next, IMGDATA_FRAMES);
    }
    tpool_add_task(preloader, NULL, NULL);
}

/**
 * Set currently displayed image.
 * @param img image to show
 */
static void set_current_image(struct image* img)
{
    struct image* prev = ctx.vp.image;

    assert(image_has_frames(img));
    assert(imglist_is_locked());

    // switch image
    viewport_reset(&ctx.vp, img);
    if (prev && prev != img) {
        image_free(prev, IMGDATA_FRAMES);
    }

    // update info
    info_reset(ctx.vp.image);
    info_update_index(info_index, ctx.vp.image->index, imglist_size());
    info_update(info_scale, "%.0f%%", ctx.vp.scale * 100);

    // update window props
    ui_set_title(ctx.vp.image->name);
    ui_set_ctype(viewport_anim_stat(&ctx.vp));

    // add task to load next image
    start_preloader();

    // restart timer
    if (ctx.enabled) {
        timer_ctl(true);
    }

    app_redraw();
}

/**
 * Open nearest image to the current one.
 * @param direction next image position
 * @return true if next image is opened
 */
static bool open_nearest_image(enum action_type direction)
{
    struct image* img;

    assert(imglist_is_locked());

    switch (direction) {
        case action_first_file:
            img = imglist_first();
            break;
        case action_last_file:
            img = imglist_last();
            break;
        case action_prev_dir:
            img = imglist_prev_parent(ctx.vp.image, true);
            break;
        case action_next_dir:
            img = imglist_next_parent(ctx.vp.image, true);
            break;
        case action_prev_file:
            img = imglist_prev(ctx.vp.image, true);
            break;
        case action_next_file:
            img = imglist_next(ctx.vp.image, true);
            break;
        case action_rand_file:
            img = imglist_rand(ctx.vp.image);
            break;
        default:
            assert(false && "unreachable code");
            return false;
    }

    while (img) {
        if (image_has_frames(img) || image_load(img) == imgload_success) {
            break;
        } else {
            struct image* skip = img;
            switch (direction) {
                case action_first_file:
                case action_next_dir:
                case action_next_file:
                case action_rand_file:
                    img = imglist_next(img, true);
                    break;
                default:
                    img = imglist_prev(img, true);
                    break;
            }
            imglist_remove(skip);
        }
    }

    if (img == ctx.vp.image) {
        img = NULL; // no more images
    }
    if (img) {
        set_current_image(img);
    }

    return !!img;
}

/** Slideshow timer event handler. */
static void on_slideshow_timer(__attribute__((unused)) void* data)
{
    if (!ctx.next) {
        tpool_wait();
    }
    imglist_lock();
    if (ctx.next) {
        set_current_image(ctx.next);
    } else if (!open_nearest_image(action_next_file) && ctx.enabled) {
        timer_ctl(true); // restart timer
    }
    imglist_unlock();
}

/** Animation frame switch handler. */
static void on_animation(void)
{
    const struct pixmap* pm = viewport_pixmap(&ctx.vp);
    const size_t max_frames = ctx.vp.image->data->frames->size;
    info_update_index(info_frame, ctx.vp.frame + 1, max_frames);
    info_update(info_image_size, "%zux%zu", pm->width, pm->height);
    app_redraw();
}

/** Redraw window. */
static void redraw(void)
{
    struct pixmap* wnd = ui_draw_begin();
    if (wnd) {
        viewport_draw(&ctx.vp, wnd);
        info_print(wnd);
        ui_draw_commit();
    }
}

/** Mode handler: window resize. */
static void on_resize(void)
{
    viewport_resize(&ctx.vp, ui_get_width(), ui_get_height());
}

/** Mode handler: image list update. */
static void on_imglist(struct image* image, enum fsevent event)
{
    bool force_next = false;

    switch (event) {
        case fsevent_create:
            break; // ignore
        case fsevent_modify:
            if (image == ctx.vp.image) {
                // reload current file
                if (image_load(image) == imgload_success) {
                    set_current_image(image);
                } else {
                    force_next = true;
                }
            }
            break;
        case fsevent_remove:
            if (image == ctx.vp.image) {
                force_next = true;
            }
            if (image == ctx.next) {
                ctx.next = NULL;
            }
            break;
    }

    if (force_next && !open_nearest_image(action_next_file)) {
        fprintf(stderr, "No more images to view, exit\n");
        app_exit(0);
    }
}

/** Mode handler: apply action. */
static bool handle_action(const struct action* action)
{
    switch (action->type) {
        case action_first_file:
        case action_last_file:
        case action_prev_dir:
        case action_next_dir:
        case action_prev_file:
        case action_next_file:
        case action_rand_file:
            imglist_lock();
            open_nearest_image(action->type);
            imglist_unlock();
            break;
        case action_redraw:
            redraw();
            break;
        case action_pause:
            ctx.enabled = !ctx.enabled;
            timer_ctl(ctx.enabled);
            info_update(info_status, ctx.enabled ? "Continue" : "Pause");
            app_redraw();
            break;
        default:
            return false;
    }
    return true;
}

/** Mode handler: get currently viewed image. */
static struct image* get_current(void)
{
    return ctx.vp.image;
}

/** Mode handler: activate viewer. */
static void on_activate(struct image* image)
{
    ctx.enabled = true;

    imglist_lock();

    if (image_has_frames(image) || image_load(image) == imgload_success) {
        on_resize();
        set_current_image(image);
    }

    imglist_unlock();
}

/** Mode handler: deactivate viewer. */
static void on_deactivate(void)
{
    viewport_reset(&ctx.vp, NULL);
    timer_ctl(false);
    ui_set_ctype(false);

    tpool_wait();
    if (ctx.next) {
        image_free(ctx.next, IMGDATA_FRAMES);
        ctx.next = NULL;
    }
}

/** Mode handler: get key bindings. */
static struct keybind* get_keybinds(void)
{
    return ctx.kb;
}

void slideshow_init(const struct config* cfg, struct mode* handlers)
{
    const struct config* section = config_section(cfg, CFG_SLIDESHOW);

    // init viewport
    viewport_init(&ctx.vp, section);
    ctx.vp.animation_cb = on_animation;

    // load key bindings
    ctx.kb = keybind_load(config_section(cfg, CFG_KEYS_SLIDESHOW));

    // setup slideshow timer
    ctx.duration = config_get_num(section, CFG_VIEW_SSHOW_TM, 1, 86400);
    ctx.timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (ctx.timer_fd != -1) {
        app_watch(ctx.timer_fd, on_slideshow_timer, NULL);
    }

    handlers->on_activate = on_activate;
    handlers->on_deactivate = on_deactivate;
    handlers->on_resize = on_resize;
    handlers->on_imglist = on_imglist;
    handlers->handle_action = handle_action;
    handlers->get_current = get_current;
    handlers->get_keybinds = get_keybinds;
}

void slideshow_destroy(void)
{
    if (ctx.timer_fd != -1) {
        close(ctx.timer_fd);
    }

    keybind_free(ctx.kb);
    viewport_free(&ctx.vp);
}
