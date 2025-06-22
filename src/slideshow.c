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
    struct image* img;
    assert(!ctx.next);

    imglist_lock();

    img = imglist_next(ctx.vp.image);
    if (!img) {
        img = imglist_first();
    }
    while (img && !ctx.next) {
        if (image_has_frames(img) || image_load(img) == imgload_success) {
            ctx.next = img;
        } else {
            struct image* skip = img;
            img = imglist_next(skip);
            if (!img) {
                img = imglist_prev(skip);
            }
            imglist_remove(skip);
            if (!img) {
                img = imglist_first();
            }
        }
    }

    imglist_unlock();
}

/**
 * Switch to the next image.
 * @param img next image to show
 */
static void switch_image(struct image* img)
{
    struct image* curr = ctx.vp.image;

    assert(ctx.vp.image != img);
    assert(image_has_frames(img));

    // switch image
    viewport_reset(&ctx.vp, img);
    if (curr) {
        image_free(curr, IMGDATA_FRAMES);
    }

    // update info
    info_reset(ctx.vp.image);
    info_update_index(info_index, ctx.vp.image->index, imglist_size());
    info_update(info_scale, "%.0f%%", ctx.vp.scale * 100);

    // update window props
    ui_set_title(ctx.vp.image->name);
    ui_set_ctype(viewport_anim_stat(&ctx.vp));

    // add task to load next image
    assert(!ctx.next);
    tpool_add_task(preloader, NULL, NULL);

    // restart timer
    timer_ctl(true);
}

/** Slideshow timer event handler. */
static void on_slideshow_timer(__attribute__((unused)) void* data)
{
    if (!ctx.next) {
        tpool_wait();
    }
    if (ctx.next) {
        struct image* img = ctx.next;
        ctx.next = NULL;
        switch_image(img);
        app_redraw();
    } else {
        fprintf(stderr, "No more images to view, exit\n");
        app_exit(0);
    }
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

/** Mode handler: apply action. */
static void handle_action(const struct action* action)
{
    switch (action->type) {
        case action_redraw:
            redraw();
            break;
        default:
            break;
    }
}

/** Mode handler: get currently viewed image. */
static struct image* get_current(void)
{
    return ctx.vp.image;
}

/** Mode handler: activate viewer. */
static void on_activate(struct image* image)
{
    if (image_has_frames(image) || image_load(image) == imgload_success) {
        on_resize();
        switch_image(image);
    }
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
