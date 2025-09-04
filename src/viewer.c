// SPDX-License-Identifier: MIT
// Image viewer mode.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "viewer.h"

#include "application.h"
#include "array.h"
#include "cache.h"
#include "imglist.h"
#include "info.h"
#include "render.h"
#include "ui/ui.h"
#include "viewport.h"

#include <pthread.h>
#include <string.h>

/** Viewer context. */
struct viewer {
    struct viewport vp; ///< Viewport
    struct keybind* kb; ///< Key bindings

    struct cache* history;   ///< Recently viewed images
    struct cache* preload;   ///< Preloaded images
    pthread_t preload_tid;   ///< Preload thread id
    bool preload_active;     ///< Preload in progress flag
    bool loop_list;          ///< Loop image list
    size_t mouse_x, mouse_y; ///< Last known mouse position, SIZE_MAX if unknown
};

/** Global viewer context. */
static struct viewer ctx;

/**
 * Preloader thread.
 */
static void* preloader_thread(__attribute__((unused)) void* data)
{
    struct image* current = ctx.vp.image;
    size_t counter = 0;

    while (ctx.preload_active && counter < cache_capacity(ctx.preload)) {
        struct image* next;
        struct image* origin;
        enum image_status status;

        imglist_lock();

        if (current != ctx.vp.image) {
            current = ctx.vp.image;
            counter = 0;
        }

        // get next image
        next = imglist_jump(ctx.vp.image, counter + 1);
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
 * Reset state to defaults.
 */
static void reset_state(void)
{
    info_reset(ctx.vp.image);
    info_update_index(info_index, ctx.vp.image->index, imglist_size());
    info_update(info_scale, "%.0f%%", ctx.vp.scale * 100);

    ui_set_title(ctx.vp.image->name);
    ui_set_ctype(viewport_anim_stat(&ctx.vp));

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
        next = forward ? imglist_next(img, ctx.loop_list)
                       : imglist_prev(img, ctx.loop_list);
        if (next == ctx.vp.image) {
            next = NULL;
        } else {
            imglist_remove(img);
        }
        img = next;
    }

    if (img) {
        if (!cache_put(ctx.history, ctx.vp.image)) {
            image_free(ctx.vp.image, IMGDATA_FRAMES);
        }
        viewport_reset(&ctx.vp, img);
        preloader_start();
        reset_state();
    } else {
        info_update_index(info_index, ctx.vp.image->index, imglist_size());
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
            next = imglist_prev_parent(ctx.vp.image, ctx.loop_list);
            break;
        case action_next_dir:
            next = imglist_next_parent(ctx.vp.image, ctx.loop_list);
            forward = true;
            break;
        case action_prev_file:
            next = imglist_prev(ctx.vp.image, ctx.loop_list);
            break;
        case action_next_file:
            next = imglist_next(ctx.vp.image, ctx.loop_list);
            forward = true;
            break;
        case action_rand_file:
            next = imglist_rand(ctx.vp.image);
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
 * Switch to the next/previous frame.
 * @param forward switch direction (next/previous)
 */
static void switch_frame(bool forward)
{
    const struct pixmap* pm = viewport_pixmap(&ctx.vp);
    const size_t max_frames = ctx.vp.image->data->frames->size;

    viewport_anim_ctl(&ctx.vp, vp_actl_stop);
    viewport_frame(&ctx.vp, forward);
    ui_set_ctype(false);

    if (max_frames > 1) {
        info_update_index(info_frame, ctx.vp.frame + 1, max_frames);
    }
    info_update(info_image_size, "%zux%zu", pm->width, pm->height);
    app_redraw();
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

/**
 * Skip current image file.
 * @param remove flag to remove current image from the image list
 * @return true if next image opened
 */
static bool skip_current(bool remove)
{
    struct image* curr = ctx.vp.image;
    struct image* next;

    next = imglist_next(ctx.vp.image, false);
    next = open_image(next, true);
    if (!next) {
        next = imglist_prev(ctx.vp.image, false);
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
    if (image_load(ctx.vp.image) == imgload_success) {
        viewport_reset(&ctx.vp, ctx.vp.image);
        info_update(info_status, "Image reloaded");
        reset_state();
    } else {
        info_update(info_status, "Unable to reload file, open next one");
        skip_current(true);
    }
}

/** Redraw window. */
static void redraw(void)
{
    struct pixmap* wnd = ui_draw_begin();
    if (wnd) {

// #define TRACE_DRAW_TIME
#ifdef TRACE_DRAW_TIME
        double ns;
        struct timespec begin, end;
        clock_gettime(CLOCK_MONOTONIC, &begin);
#endif

        viewport_draw(&ctx.vp, wnd);
        info_print(wnd);

#ifdef TRACE_DRAW_TIME
        clock_gettime(CLOCK_MONOTONIC, &end);
        ns = (double)((end.tv_sec * 1000000000 + end.tv_nsec) -
                      (begin.tv_sec * 1000000000 + begin.tv_nsec));
        printf("Redraw in %.6f sec\n", ns / 1000000000);
#endif

        ui_draw_commit();
    }
}

/**
 * Move image: handle "move" action.
 * @param dir move direction
 * @param params action parameters
 */
static void move_image(enum vp_move dir, const char* params)
{
    ssize_t step = 10; // in %

    if (*params) {
        ssize_t val;
        if (str_to_num(params, 0, &val, 0) && val > 0 && val <= 1000) {
            step = val;
        } else {
            info_update(info_status, "Invalid move step: \"%s\"\n", params);
        }
    }

    // convert % to px
    switch (dir) {
        case vp_move_up:
        case vp_move_down:
            step *= ui_get_height() / 100;
            break;
        case vp_move_left:
        case vp_move_right:
            step *= ui_get_width() / 100;
            break;
    }

    // invert direction for viewport
    switch (dir) {
        case vp_move_up:
            dir = vp_move_down;
            break;
        case vp_move_down:
            dir = vp_move_up;
            break;
        case vp_move_left:
            dir = vp_move_right;
            break;
        case vp_move_right:
            dir = vp_move_left;
            break;
    }

    viewport_move(&ctx.vp, dir, step);
    app_redraw();
}

/**
 * Zoom image: handle "zoom" action.
 * @param params action parameters
 */
static void zoom_image(const char* params)
{
    const char* name = NULL;

    if (!*params) {
        name = viewport_scale_switch(&ctx.vp);
    } else if (viewport_scale_def(&ctx.vp, params)) {
        name = params;
    } else {
        struct str_slice ps[2];
        size_t ps_num;

        double scale = 0.0;

        ps_num = str_split(params, ' ', ps, ARRAY_SIZE(ps));

        if (params[0] == '+' || params[0] == '-') {
            // relative
            ssize_t delta;
            if (str_to_num(ps[0].value, ps[0].len, &delta, 0) && delta != 0 &&
                delta > -1000 && delta < 1000) {
                scale = ctx.vp.scale + (ctx.vp.scale / 100) * delta;
            }
        } else {
            // percent
            ssize_t percent;
            if (str_to_num(ps[0].value, ps[0].len, &percent, 0) &&
                percent > 0 && percent < 100000) {
                scale = (double)percent / 100;
            }
        }

        if (scale != 0) {
            size_t preserve_x = SIZE_MAX, preserve_y = SIZE_MAX;
            // If mouse is unsupported (e.g. DRM backend or no mouse movement
            // yet), use center position instead.
            if (ps_num == 1 || strncmp(ps[1].value, "center", ps[1].len) == 0 ||
                ctx.mouse_x == SIZE_MAX || ctx.mouse_y == SIZE_MAX) {
                preserve_x = ctx.vp.width / 2.0;
                preserve_y = ctx.vp.height / 2.0;
            } else if (strncmp(ps[1].value, "mouse", ps[1].len) == 0) {
                preserve_x = ctx.mouse_x;
                preserve_y = ctx.mouse_y;
            }

            if (preserve_x != SIZE_MAX) {
                viewport_scale_abs(&ctx.vp, scale, preserve_x, preserve_y);
                info_update(info_scale, "%.0f%%", ctx.vp.scale * 100);
            } else {
                info_update(info_status, "Invalid second zoom parameter: %.*s",
                            (int)ps[1].len, ps[1].value);
            }
        } else {
            info_update(info_status, "Invalid zoom operation: %s", params);
        }
    }

    if (name) {
        info_update(info_status, "Scale mode: %s", name);
    }
    app_redraw();
}

/**
 * Position image: handle "position" action.
 * @param params action parameters
 */
static void position_image(const char* params)
{
    if (viewport_position_def(&ctx.vp, params)) {
        info_update(info_status, "Position: %s", params);
    } else {
        info_update(info_status, "Invalid position: %s", params);
    }
    app_redraw();
}

/**
 * Switch antialiasing mode: handle "antialiasing" action.
 * @param params action parameters
 */
static void switch_antialiasing(const char* params)
{
    if (*params) {
        if (aa_from_name(params, &ctx.vp.aa)) {
            info_update(info_status, "Anti-aliasing: %s", params);
        } else {
            info_update(info_status, "Invalid anti-aliasing: %s", params);
        }
    } else {
        ctx.vp.aa_en = !ctx.vp.aa_en;
        info_update(info_status, "Anti-aliasing: %s",
                    ctx.vp.aa_en ? "ON" : "OFF");
    }
    app_redraw();
}

/** Mode handler: window resize. */
static void on_resize(void)
{
    viewport_resize(&ctx.vp, ui_get_width(), ui_get_height());
    reset_state();
}

/** Mode handler: image list update. */
static void on_imglist(struct image* image, enum fsevent event)
{
    switch (event) {
        case fsevent_create:
            break;
        case fsevent_modify:
            if (image == ctx.vp.image) {
                reload_current();
            } else {
                cache_out(ctx.preload, image);
                cache_out(ctx.history, image);
            }
            break;
        case fsevent_remove:
            if (image == ctx.vp.image) {
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
    ctx.mouse_x = x;
    ctx.mouse_y = y;

    const struct keybind* kb = keybind_find(ctx.kb, MOUSE_TO_XKB(btn), mods);
    if (kb && kb->actions->type == action_drag) {
        // move viewport
        if (dx > 0) {
            viewport_move(&ctx.vp, vp_move_right, dx);
        } else if (dx < 0) {
            viewport_move(&ctx.vp, vp_move_left, -dx);
        }
        if (dy > 0) {
            viewport_move(&ctx.vp, vp_move_down, dy);
        } else if (dy < 0) {
            viewport_move(&ctx.vp, vp_move_up, -dy);
        }
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
            next_image(action->type);
            break;
        case action_skip_file:
            imglist_lock();
            skip_current(true);
            imglist_unlock();
            break;
        case action_prev_frame:
        case action_next_frame:
            switch_frame(action->type == action_next_frame);
            break;
        case action_animation:
            viewport_anim_ctl(&ctx.vp,
                              viewport_anim_stat(&ctx.vp) ? vp_actl_stop
                                                          : vp_actl_start);
            ui_set_ctype(viewport_anim_stat(&ctx.vp));
            break;
        case action_step_left:
            move_image(vp_move_left, action->params);
            break;
        case action_step_right:
            move_image(vp_move_right, action->params);
            break;
        case action_step_up:
            move_image(vp_move_up, action->params);
            break;
        case action_step_down:
            move_image(vp_move_down, action->params);
            break;
        case action_zoom:
            zoom_image(action->params);
            break;
        case action_position:
            position_image(action->params);
            break;
        case action_rotate_left:
            image_rotate(ctx.vp.image, 270);
            viewport_rotate(&ctx.vp);
            app_redraw();
            break;
        case action_rotate_right:
            image_rotate(ctx.vp.image, 90);
            viewport_rotate(&ctx.vp);
            app_redraw();
            break;
        case action_flip_vertical:
            image_flip_vertical(ctx.vp.image);
            app_redraw();
            break;
        case action_flip_horizontal:
            image_flip_horizontal(ctx.vp.image);
            app_redraw();
            break;
        case action_antialiasing:
            switch_antialiasing(action->params);
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
            } else if (image_export(ctx.vp.image, ctx.vp.frame,
                                    action->params)) {
                info_update(info_status, "Exported to %s", action->params);
            } else {
                info_update(info_status, "Error: export failed");
            }
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
    cache_out(ctx.preload, image);
    cache_out(ctx.history, image);

    viewport_resize(&ctx.vp, ui_get_width(), ui_get_height());
    if (image_has_frames(image) || image_load(image) == imgload_success) {
        viewport_reset(&ctx.vp, image);
        reset_state();
        preloader_start();
        viewport_anim_ctl(&ctx.vp, vp_actl_start);
        ui_set_ctype(viewport_anim_stat(&ctx.vp));
    } else {
        skip_current(true);
    }
}

/** Mode handler: deactivate viewer. */
static void on_deactivate(void)
{
    preloader_stop();
    cache_put(ctx.history, ctx.vp.image);
    viewport_reset(&ctx.vp, NULL);
    ui_set_ctype(false);
}

/** Mode handler: get key bindings. */
static struct keybind* get_keybinds(void)
{
    return ctx.kb;
}

void viewer_init(const struct config* cfg, struct mode* handlers)
{
    const struct config* section = config_section(cfg, CFG_VIEWER);
    size_t value;

    // init viewport
    viewport_init(&ctx.vp, section);
    ctx.vp.animation_cb = on_animation;

    // image list loop mode
    ctx.loop_list = config_get_bool(section, CFG_VIEW_LOOP);

    // init history and preloads caches
    value = config_get_num(section, CFG_VIEW_HISTORY, 0, 1024);
    ctx.history = cache_init(value);
    value = config_get_num(section, CFG_VIEW_PRELOAD, 0, 1024);
    ctx.preload = cache_init(value);

    // load key bindings
    ctx.kb = keybind_load(config_section(cfg, CFG_KEYS_VIEWER));

    ctx.mouse_x = SIZE_MAX;
    ctx.mouse_y = SIZE_MAX;

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
    keybind_free(ctx.kb);
    cache_free(ctx.history);
    cache_free(ctx.preload);
}
