// SPDX-License-Identifier: MIT
// Image viewer application: main loop and event handler.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "application.h"

#include "array.h"
#include "fdpoll.h"
#include "font.h"
#include "gallery.h"
#include "imglist.h"
#include "info.h"
#include "ipc.h"
#include "slideshow.h"
#include "tpool.h"
#include "ui/ui.h"
#include "viewer.h"

#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

/** Mode names. */
static const char* mode_names[] = { CFG_VIEWER, CFG_SLIDESHOW, CFG_GALLERY };

/** Main loop state */
enum loop_state {
    loop_run,
    loop_stop,
    loop_error,
};

/* Event queue. */
struct event {
    struct list list;            ///< Links to prev/next entry
    const struct action* action; ///< Action to perform
    bool free_after_use;         ///< Flag to free action
};

/** Application context */
struct application {
    enum loop_state state; ///< Main loop state

    struct event* events;        ///< Event queue
    pthread_mutex_t events_lock; ///< Event queue lock
    int event_signal;            ///< Queue change notification

    struct action* sigusr1; ///< Actions applied by USR1 signal
    struct action* sigusr2; ///< Actions applied by USR2 signal

    enum mode_type mcurr;                      ///< Currently active mode
    enum mode_type mprev;                      ///< Previous mode
    struct mode modes[ARRAY_SIZE(mode_names)]; ///< Mode handlers
};

/** Global application context. */
static struct application ctx;

void app_switch_mode(const char* name)
{
    struct image* img;
    enum mode_type next = ctx.mprev;

    if (*name) {
        const ssize_t index = str_index(mode_names, name, 0);
        if (index >= 0) {
            next = index;
        } else {
            info_update(info_status, "Invalid mode: %s", name);
            app_redraw();
            return;
        }
    }
    if (next == ctx.mcurr) {
        return;
    }

    // switch mode
    img = ctx.modes[ctx.mcurr].get_current();
    ctx.modes[ctx.mcurr].on_deactivate();
    ctx.mprev = ctx.mcurr;
    ctx.mcurr = next;
    ctx.modes[ctx.mcurr].on_activate(img);

    info_set_default(mode_names[ctx.mcurr], 0);
    if (help_visible()) {
        help_hide();
    }

    app_redraw();
}

/** Notification callback: handle event queue. */
static void handle_event_queue(__attribute__((unused)) void* data)
{
    struct event* entry = NULL;

    pthread_mutex_lock(&ctx.events_lock);
    if (ctx.events) {
        entry = ctx.events;
        ctx.events = list_remove(entry);
    }
    pthread_mutex_unlock(&ctx.events_lock);

    if (!ctx.events) {
        fdevent_reset(ctx.event_signal);
    }

    if (entry) {
        mode_handle(&ctx.modes[ctx.mcurr], entry->action);
        if (entry->free_after_use) {
            action_free((struct action*)entry->action);
        }
        free(entry);
    }
}

/**
 * POSIX Signal handler.
 * @param signum signal number
 */
static void on_signal(int signum)
{
    const struct action* sigact;

    switch (signum) {
        case SIGUSR1:
            sigact = ctx.sigusr1;
            break;
        case SIGUSR2:
            sigact = ctx.sigusr2;
            break;
        default:
            return;
    }

    while (sigact) {
        app_apply_action(sigact, false);
        sigact = sigact->next;
    }
}

/**
 * Setup signal handlers.
 * @param section general config section
 */
static void setup_signals(const struct config* section)
{
    struct sigaction sigact;
    const char* value;

    // get signal actions
    value = config_get(section, CFG_GNRL_SIGUSR1);
    ctx.sigusr1 = action_create(value);
    if (!ctx.sigusr1) {
        config_error_val(section->name, CFG_GNRL_SIGUSR1);
        value = config_get_default(section->name, CFG_GNRL_SIGUSR1);
        ctx.sigusr1 = action_create(value);
    }
    value = config_get(section, CFG_GNRL_SIGUSR2);
    ctx.sigusr2 = action_create(value);
    if (!ctx.sigusr2) {
        config_error_val(section->name, CFG_GNRL_SIGUSR2);
        value = config_get_default(section->name, CFG_GNRL_SIGUSR2);
        ctx.sigusr2 = action_create(value);
    }

    // set handlers
    sigact.sa_handler = on_signal;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGUSR1, &sigact, NULL);
    sigaction(SIGUSR2, &sigact, NULL);
}

/**
 * Create image list and load the first image.
 * @param sources list of sources
 * @param num number of sources in the list
 * @return first image instance to show or NULL on errors
 */
static struct image* create_imglist(const char* const* sources, size_t num)
{
    struct image* image = imglist_load(sources, num);
    if (!image) {
        fprintf(stderr, "Image list is empty, no files to view\n");
        return NULL;
    }

    // load first image
    if (imglist_size() == 1) {
        const char* fail = NULL;
        switch (image_load(image)) {
            case imgload_success:
                break;
            case imgload_unsupported:
                fail = "Unsupported format";
                break;
            case imgload_fmterror:
                fail = "Invalid format";
                break;
            case imgload_unknown:
            default:
                fail = "Unknown error";
                break;
        }
        if (fail) {
            fprintf(stderr, "%s: %s\n", image->source, fail);
            imglist_remove(image);
            image = NULL;
        }
        return image;
    }

    // try to load first available image
    while (image && image_load(image) != imgload_success) {
        struct image* skip = image;
        image = imglist_next(skip, false);
        if (!image) {
            image = imglist_prev(skip, false);
        }
        imglist_remove(skip);
    }
    if (!image) {
        fprintf(stderr, "Unable to load any images\n");
    }

    return image;
}

bool app_init(const struct config* cfg, const char* const* sources, size_t num)
{
    const struct config* general = config_section(cfg, CFG_GENERAL);
    struct image* first_image;
    const char* ipc_path;

    // create image list
    imglist_init(cfg);
    first_image = create_imglist(sources, num);
    if (!first_image) {
        imglist_destroy();
        return false;
    }

    // create ui window
    if (!ui_init(cfg, first_image)) {
        imglist_destroy();
        return false;
    }

    // set initial mode
    ctx.mcurr = config_get_oneof(general, CFG_GNRL_MODE, mode_names,
                                 ARRAY_SIZE(mode_names));
    ctx.mprev = mode_viewer;

    // create event queue notification
    ctx.event_signal = fdevent_add(handle_event_queue, NULL);
    if (ctx.event_signal < 0) {
        fprintf(stderr, "Error creating events: [%i] %s\n", -ctx.event_signal,
                strerror(-ctx.event_signal));
        imglist_destroy();
        ui_destroy();
        return false;
    }
    pthread_mutex_init(&ctx.events_lock, NULL);

    // initialize other subsystems
    tpool_init();
    font_init(cfg);
    info_init(cfg, mode_names[ctx.mcurr]);
    viewer_init(cfg, &ctx.modes[mode_viewer]);
    slideshow_init(cfg, &ctx.modes[mode_slideshow]);
    gallery_init(cfg, &ctx.modes[mode_gallery]);

    // set signal handler
    setup_signals(general);

    // start ipc server
    ipc_path = config_get(general, CFG_GNRL_IPC);
    if (*ipc_path) {
        ipc_start(ipc_path);
    }

    ctx.modes[ctx.mcurr].on_activate(first_image);

    return true;
}

void app_destroy(void)
{
    ipc_stop();
    gallery_destroy();
    slideshow_destroy();
    viewer_destroy();
    ui_destroy();
    imglist_destroy();
    info_destroy();
    font_destroy();
    tpool_destroy();
    fdpoll_destroy();

    list_for_each(ctx.events, struct event, it) {
        free(it);
    }

    pthread_mutex_destroy(&ctx.events_lock);

    action_free(ctx.sigusr1);
    action_free(ctx.sigusr2);
}

bool app_run(void)
{
    ctx.state = loop_run;

    // main event loop
    while (ctx.state == loop_run) {
        int rc;

        ui_event_prepare();

        rc = fdpoll_next();
        if (rc) {
            fprintf(stderr, "Error polling events: [%i] %s\n", rc,
                    strerror(rc));
            ctx.state = loop_error;
            break;
        }

        ui_event_done();
    }

    ctx.modes[ctx.mcurr].on_deactivate();

    return ctx.state != loop_error;
}

void app_exit(int rc)
{
    ctx.state = rc ? loop_error : loop_stop;
}

void app_reload(void)
{
    static const struct action action = { .type = action_reload };
    app_apply_action(&action, false);
}

void app_redraw(void)
{
    static const struct action action = { .type = action_redraw };
    app_apply_action(&action, false);
}

void app_on_imglist(struct image* image, enum fsevent event)
{
    if (ctx.modes[ctx.mcurr].on_imglist) {
        ctx.modes[ctx.mcurr].on_imglist(image, event);
    }
}

void app_on_resize(void)
{
    ctx.modes[ctx.mcurr].on_resize();
}

void app_on_scale(double scale)
{
    font_set_scale(scale);
    info_reinit();
}

void app_on_mmove(uint8_t mods, uint32_t btn, size_t x, size_t y, ssize_t dx,
                  ssize_t dy)
{
    if (ctx.modes[ctx.mcurr].on_mouse_move) {
        ctx.modes[ctx.mcurr].on_mouse_move(mods, btn, x, y, dx, dy);
    }
}

void app_on_mclick(uint8_t mods, uint32_t btn, size_t x, size_t y)
{
    if (!ctx.modes[ctx.mcurr].on_mouse_click ||
        !ctx.modes[ctx.mcurr].on_mouse_click(mods, btn, x, y)) {
        app_on_keyboard(MOUSE_TO_XKB(btn), mods);
    }
}

void app_on_keyboard(xkb_keysym_t key, uint8_t mods)
{
    const struct keybind* kb =
        keybind_find(ctx.modes[ctx.mcurr].get_keybinds(), key, mods);

    if (kb) {
        const struct action* action = kb->actions;
        while (action) {
            app_apply_action(action, false);
            action = action->next;
        }
    } else {
        char* name = keybind_name(key, mods);
        if (name) {
            info_update(info_status, "%s is not bound", name);
            free(name);
            app_redraw();
        }
    }
}

void app_apply_action(const struct action* action, bool fau)
{
    struct event* event = NULL;

    pthread_mutex_lock(&ctx.events_lock);

    if (action->type == action_redraw) {
        // remove the same event to append new one to the tail
        list_for_each(ctx.events, struct event, it) {
            if (it->action->type == action_redraw) {
                if (list_is_last(it)) {
                    pthread_mutex_unlock(&ctx.events_lock);
                    return;
                }
                ctx.events = list_remove(it);
                event = it;
                break;
            }
        }
    }

    if (!event) {
        // create new entry
        event = calloc(1, sizeof(*event));
        if (!event) {
            return;
        }
        event->action = action;
        event->free_after_use = fau;
    }
    ctx.events = list_append(ctx.events, event);

    pthread_mutex_unlock(&ctx.events_lock);

    // raise notification
    fdevent_set(ctx.event_signal);
}
