// SPDX-License-Identifier: MIT
// Image viewer application: main loop and event handler.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "application.h"

#include "array.h"
#include "buildcfg.h"
#include "compositor.h"
#include "font.h"
#include "gallery.h"
#include "imglist.h"
#include "info.h"
#include "shellcmd.h"
#include "slideshow.h"
#include "tpool.h"
#include "ui.h"
#include "viewer.h"

#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <unistd.h>

// Special ids for windows size and position
#define SIZE_FULLSCREEN  SIZE_MAX
#define SIZE_FROM_IMAGE  (SIZE_MAX - 1)
#define SIZE_FROM_PARENT (SIZE_MAX - 2)
#define POS_FROM_PARENT  SSIZE_MAX

/** Mode names. */
static const char* mode_names[] = { CFG_VIEWER, CFG_SLIDESHOW, CFG_GALLERY };

/** Main loop state */
enum loop_state {
    loop_run,
    loop_stop,
    loop_error,
};

/** File descriptor and its handler. */
struct watchfd {
    int fd;
    void* data;
    fd_callback callback;
};

/* Event queue. */
struct event {
    struct list list;            ///< Links to prev/next entry
    const struct action* action; ///< Action to perform
};

/** Application context */
struct application {
    enum loop_state state; ///< Main loop state

    struct watchfd* wfds; ///< FD polling descriptors
    size_t wfds_num;      ///< Number of polling FD

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

    // switch mode
    img = ctx.modes[ctx.mcurr].get_current();
    ctx.modes[ctx.mcurr].on_deactivate();
    ctx.mcurr = next;
    ctx.modes[ctx.mcurr].on_activate(img);

    info_set_default(mode_names[ctx.mcurr]);
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
        // reset notification
        uint64_t value;
        ssize_t len;
        do {
            len = read(ctx.event_signal, &value, sizeof(value));
        } while (len == -1 && errno == EINTR);
    }

    if (entry) {
        mode_handle(&ctx.modes[ctx.mcurr], entry->action);
        free(entry);
    }
}

/**
 * Append event to queue.
 * @param action pointer to the action
 */
static void append_event(const struct action* action)
{
    struct event* event = NULL;
    const uint64_t value = 1;
    ssize_t len;

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
        event = malloc(sizeof(*event));
        if (!event) {
            return;
        }
        event->action = action;
    }

    // add to queue tail
    ctx.events = list_append(ctx.events, event);

    pthread_mutex_unlock(&ctx.events_lock);

    // raise notification
    do {
        len = write(ctx.event_signal, &value, sizeof(value));
    } while (len == -1 && errno == EINTR);
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
        append_event(sigact);
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
        image = imglist_next(skip);
        if (!image) {
            image = imglist_prev(skip);
        }
        imglist_remove(skip);
    }
    if (!image) {
        fprintf(stderr, "Unable to load any images\n");
    }

    return image;
}

/**
 * Create window.
 * @param section general config section
 * @param img first image instance
 * @return true if operation completed successfully
 */
static bool create_window(const struct config* section, const struct image* img)
{
    char* app_id = NULL;
    struct wndrect wnd;
    bool decoration;
    const char* value;

    // initial window position
    wnd.x = POS_FROM_PARENT;
    wnd.y = POS_FROM_PARENT;
    value = config_get(section, CFG_GNRL_POSITION);
    if (strcmp(value, CFG_FROM_PARENT) != 0) {
        struct str_slice slices[2];
        ssize_t x, y;
        if (str_split(value, ',', slices, 2) == 2 &&
            str_to_num(slices[0].value, slices[0].len, &x, 0) &&
            str_to_num(slices[1].value, slices[1].len, &y, 0)) {
            wnd.x = x;
            wnd.y = y;
        } else {
            config_error_val(section->name, CFG_GNRL_POSITION);
        }
    }

    // initial window size
    value = config_get(section, CFG_GNRL_SIZE);
    if (strcmp(value, CFG_FROM_PARENT) == 0) {
        wnd.width = SIZE_FROM_PARENT;
        wnd.height = SIZE_FROM_PARENT;
    } else if (strcmp(value, CFG_FROM_IMAGE) == 0) {
        wnd.width = SIZE_FROM_IMAGE;
        wnd.height = SIZE_FROM_IMAGE;
    } else if (strcmp(value, CFG_FULLSCREEN) == 0) {
        wnd.width = SIZE_FULLSCREEN;
        wnd.height = SIZE_FULLSCREEN;
    } else {
        ssize_t width, height;
        struct str_slice slices[2];
        if (str_split(value, ',', slices, 2) == 2 &&
            str_to_num(slices[0].value, slices[0].len, &width, 0) &&
            str_to_num(slices[1].value, slices[1].len, &height, 0) &&
            width > 0 && width < 100000 && height > 0 && height < 100000) {
            wnd.width = width;
            wnd.height = height;
        } else {
            wnd.width = SIZE_FROM_PARENT;
            wnd.height = SIZE_FROM_PARENT;
            config_error_val(section->name, CFG_GNRL_SIZE);
        }
    }

    // app id (class name)
    value = config_get(section, CFG_GNRL_APP_ID);
    if (!*value) {
        config_error_val(CFG_GENERAL, CFG_GNRL_APP_ID);
        value = config_get_default(CFG_GENERAL, CFG_GNRL_APP_ID);
    }
    str_dup(value, &app_id);

    // window decoration (title/borders/...)
    decoration = config_get_bool(section, CFG_GNRL_DECOR);

    // setup window position and size
    if (wnd.width == SIZE_FULLSCREEN) {
        wnd.width = UI_WINDOW_DEFAULT_WIDTH;
        wnd.height = UI_WINDOW_DEFAULT_HEIGHT;
        ui_toggle_fullscreen();
    } else {
#ifdef HAVE_COMPOSITOR
        bool compositor = config_get_bool(section, CFG_GNRL_WC);
        if (compositor) {
            struct wndrect focus;
            compositor = compositor_get_focus(&focus);
            if (compositor) {
                if (wnd.x == POS_FROM_PARENT && wnd.y == POS_FROM_PARENT) {
                    wnd.x = focus.x;
                    wnd.y = focus.y;
                }
                if (wnd.width == SIZE_FROM_PARENT &&
                    wnd.height == SIZE_FROM_PARENT) {
                    wnd.width = focus.width;
                    wnd.height = focus.height;
                }
            }
        }
#else
        bool compositor = false;
#endif // HAVE_COMPOSITOR

        if (wnd.width == SIZE_FROM_PARENT && wnd.height == SIZE_FROM_PARENT) {
            // fallback if compositor not available
            wnd.width = SIZE_FROM_IMAGE;
            wnd.height = SIZE_FROM_IMAGE;
        }
        if (wnd.width == SIZE_FROM_IMAGE && wnd.height == SIZE_FROM_IMAGE) {
            // determine window size from the first frame of the first image
            struct imgframe* frame = arr_nth(img->data->frames, 0);
            wnd.width = frame->pm.width;
            wnd.height = frame->pm.height;
        }

        // limit window size
        if (wnd.width < UI_WINDOW_MIN || wnd.height < UI_WINDOW_MIN ||
            wnd.width * wnd.height > UI_WINDOW_MAX) {
            wnd.width = UI_WINDOW_DEFAULT_WIDTH;
            wnd.height = UI_WINDOW_DEFAULT_HEIGHT;
        }

#ifdef HAVE_COMPOSITOR
        if (compositor) {
            compositor_overlay(&wnd, &app_id);
        }
#endif // HAVE_COMPOSITOR
    }

    if (!ui_init(app_id, wnd.width, wnd.height, decoration)) {
        free(app_id);
        return false;
    }

    free(app_id);
    return true;
}

bool app_init(const struct config* cfg, const char* const* sources, size_t num)
{
    const struct config* general = config_section(cfg, CFG_GENERAL);
    struct image* first_image;

    // create image list
    imglist_init(cfg);
    first_image = create_imglist(sources, num);
    if (!first_image) {
        imglist_destroy();
        return false;
    }

    // create ui window
    if (!create_window(general, first_image)) {
        imglist_destroy();
        return false;
    }

    // set initial mode
    ctx.mcurr = config_get_oneof(general, CFG_GNRL_MODE, mode_names,
                                 ARRAY_SIZE(mode_names));
    ctx.mprev = ctx.mcurr;

    // create event queue notification
    ctx.event_signal = eventfd(0, 0);
    if (ctx.event_signal != -1) {
        app_watch(ctx.event_signal, handle_event_queue, NULL);
    } else {
        perror("Unable to create eventfd");
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

    ctx.modes[ctx.mcurr].on_activate(first_image);

    return true;
}

void app_destroy(void)
{
    gallery_destroy();
    slideshow_destroy();
    viewer_destroy();
    ui_destroy();
    imglist_destroy();
    info_destroy();
    font_destroy();
    tpool_destroy();

    for (size_t i = 0; i < ctx.wfds_num; ++i) {
        close(ctx.wfds[i].fd);
    }
    free(ctx.wfds);

    list_for_each(ctx.events, struct event, it) {
        free(it);
    }

    if (ctx.event_signal != -1) {
        close(ctx.event_signal);
    }
    pthread_mutex_destroy(&ctx.events_lock);

    action_free(ctx.sigusr1);
    action_free(ctx.sigusr2);
}

void app_watch(int fd, fd_callback cb, void* data)
{
    const size_t sz = (ctx.wfds_num + 1) * sizeof(*ctx.wfds);
    struct watchfd* handlers = realloc(ctx.wfds, sz);
    if (handlers) {
        ctx.wfds = handlers;
        ctx.wfds[ctx.wfds_num].fd = fd;
        ctx.wfds[ctx.wfds_num].data = data;
        ctx.wfds[ctx.wfds_num].callback = cb;
        ++ctx.wfds_num;
    }
}

bool app_run(void)
{
    struct pollfd* fds;

    // file descriptors to poll
    fds = calloc(1, ctx.wfds_num * sizeof(struct pollfd));
    if (!fds) {
        perror("Failed to allocate memory");
        return false;
    }
    for (size_t i = 0; i < ctx.wfds_num; ++i) {
        fds[i].fd = ctx.wfds[i].fd;
        fds[i].events = POLLIN;
    }

    // main event loop
    ctx.state = loop_run;
    while (ctx.state == loop_run) {
        ui_event_prepare();

        // poll events
        if (poll(fds, ctx.wfds_num, -1) < 0) {
            if (errno != EINTR) {
                perror("Error polling events");
                ctx.state = loop_error;
                break;
            }
        }

        // call handlers for each active event
        for (size_t i = 0; ctx.state == loop_run && i < ctx.wfds_num; ++i) {
            if (fds[i].revents & POLLIN) {
                ctx.wfds[i].callback(ctx.wfds[i].data);
            }
        }

        ui_event_done();
    }

    ctx.modes[ctx.mcurr].on_deactivate();

    free(fds);

    return ctx.state != loop_error;
}

void app_exit(int rc)
{
    ctx.state = rc ? loop_error : loop_stop;
}

bool app_is_viewer(void)
{
    return ctx.mcurr == mode_viewer;
}

void app_reload(void)
{
    static const struct action action = { .type = action_reload };
    append_event(&action);
}

void app_redraw(void)
{
    static const struct action action = { .type = action_redraw };
    append_event(&action);
}

void app_on_imglist(const struct image* image, enum fsevent event)
{
    if (ctx.modes[ctx.mcurr].on_imglist) {
        ctx.modes[ctx.mcurr].on_imglist(image, event);
    }
}

void app_on_resize(void)
{
    ctx.modes[ctx.mcurr].on_resize();
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
            append_event(action);
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
