// SPDX-License-Identifier: MIT
// Image viewer application: main loop and event handler.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "application.h"

#include "buildcfg.h"
#include "config.h"
#include "font.h"
#include "gallery.h"
#include "imagelist.h"
#include "info.h"
#include "loader.h"
#include "sway.h"
#include "ui.h"
#include "viewer.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// Special ids for windows size and position
#define SIZE_FULLSCREEN  SIZE_MAX
#define SIZE_FROM_IMAGE  (SIZE_MAX - 1)
#define SIZE_FROM_PARENT (SIZE_MAX - 2)
#define POS_FROM_PARENT  SSIZE_MAX

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

/* Application event queue (list). */
struct event_queue {
    struct list list;
    struct event event;
};

/** Application context */
struct application {
    enum loop_state state; ///< Main loop state

    struct watchfd* wfds; ///< FD polling descriptors
    size_t wfds_num;      ///< Number of polling FD

    struct event_queue* events;  ///< Event queue
    pthread_mutex_t events_lock; ///< Event queue lock
    int event_signal;            ///< Queue change notification

    struct action_seq sigusr1; ///< Actions applied by USR1 signal
    struct action_seq sigusr2; ///< Actions applied by USR2 signal

    event_handler ehandler; ///< Event handler for the current mode
    struct wndrect window;  ///< Preferable window position and size
    char* app_id;           ///< Application id (app_id name)
};

/** Global application context. */
static struct application ctx;

/**
 * Setup window position via Sway IPC.
 */
static void sway_setup(void)
{
    struct wndrect parent;
    bool fullscreen;
    bool absolute = false;
    int ipc;

    ipc = sway_connect();
    if (ipc == INVALID_SWAY_IPC) {
        return; // sway not available
    }
    if (!sway_current(ipc, &parent, &fullscreen)) {
        sway_disconnect(ipc);
        return;
    }

    if (fullscreen) {
        ctx.window.width = SIZE_FULLSCREEN;
        ctx.window.height = SIZE_FULLSCREEN;
        sway_disconnect(ipc);
        return;
    }

    if (ctx.window.width == SIZE_FROM_PARENT) {
        ctx.window.width = parent.width;
        ctx.window.height = parent.height;
    }
    if (ctx.window.x == POS_FROM_PARENT) {
        absolute = false;
        ctx.window.x = parent.x;
        ctx.window.y = parent.y;
    }

    // set window position via sway rules
    sway_add_rules(ipc, ctx.window.x, ctx.window.y, absolute);

    sway_disconnect(ipc);
}

/** Notification callback: handle event queue. */
static void handle_event_queue(__attribute__((unused)) void* data)
{
    notification_reset(ctx.event_signal);

    while (ctx.events && ctx.state == loop_run) {
        struct event_queue* entry = NULL;
        pthread_mutex_lock(&ctx.events_lock);
        if (ctx.events) {
            entry = ctx.events;
            ctx.events = list_remove(entry);
        }
        pthread_mutex_unlock(&ctx.events_lock);
        if (entry) {
            ctx.ehandler(&entry->event);
            free(entry);
        }
    }
}

/**
 * Append event to queue.
 * @param event pointer to the event
 */
static void append_event(const struct event* event)
{
    struct event_queue* entry;

    // create new entry
    entry = malloc(sizeof(*entry));
    if (!entry) {
        return;
    }
    memcpy(&entry->event, event, sizeof(entry->event));

    // add to queue tail
    pthread_mutex_lock(&ctx.events_lock);
    ctx.events = list_append(ctx.events, entry);
    pthread_mutex_unlock(&ctx.events_lock);

    notification_raise(ctx.event_signal);
}

/**
 * Apply action.
 * @param action pointer to the action being performed
 */
static void apply_action(const struct action* action)
{
    struct event event;

    switch (action->type) {
        case action_info:
            info_switch(action->params);
            app_redraw();
            break;
        case action_status:
            info_update(info_status, "%s", action->params);
            app_redraw();
            break;
        case action_fullscreen:
            ui_toggle_fullscreen();
            break;
        case action_help:
            info_switch_help();
            app_redraw();
            break;
        case action_exit:
            if (info_help_active()) {
                info_switch_help(); // remove help overlay
                app_redraw();
            } else {
                app_exit(0);
            }
            break;
        default:
            // not a general action, add to event queue
            event.type = event_action;
            event.param.action = action;
            append_event(&event);
            break;
    }
}

/**
 * POSIX Signal handler.
 * @param signum signal number
 */
static void on_signal(int signum)
{
    const struct action_seq* sigact;

    switch (signum) {
        case SIGUSR1:
            sigact = &ctx.sigusr1;
            break;
        case SIGUSR2:
            sigact = &ctx.sigusr2;
            break;
        default:
            return;
    }

    for (size_t i = 0; i < sigact->num; ++i) {
        apply_action(&sigact->sequence[i]);
    }
}

/**
 * Load first (initial) image.
 * @param index initial index of image in the image list
 * @param force mandatory image index flag
 * @return image instance or NULL on errors
 */
static struct image* load_first_file(size_t index, bool force)
{
    struct image* img = NULL;
    enum loader_status status = ldr_ioerror;

    if (index == IMGLIST_INVALID) {
        index = image_list_first();
        force = false;
    }

    while (index != IMGLIST_INVALID) {
        status = loader_from_index(index, &img);
        if (force || status == ldr_success) {
            break;
        }
        index = image_list_skip(index);
    }

    if (status != ldr_success) {
        // print error message
        if (!force) {
            fprintf(stderr, "No image files was loaded, exit\n");
        } else {
            const char* reason = "Unknown error";
            switch (status) {
                case ldr_success:
                    break;
                case ldr_unsupported:
                    reason = "Unsupported format";
                    break;
                case ldr_fmterror:
                    reason = "Invalid format";
                    break;
                case ldr_ioerror:
                    reason = "I/O error";
                    break;
            }
            fprintf(stderr, "%s: %s\n", image_list_get(index), reason);
        }
    }

    return img;
}

/**
 * Custom section loader, see `config_loader` for details.
 */
static enum config_status load_config(const char* key, const char* value)
{
    enum config_status status = cfgst_invalid_value;

    if (strcmp(key, APP_CFG_MODE) == 0) {
        if (strcmp(value, APP_MODE_VIEWER) == 0) {
            ctx.ehandler = viewer_handle;
            status = cfgst_ok;
        } else if (strcmp(value, APP_MODE_GALLERY) == 0) {
            ctx.ehandler = gallery_handle;
            status = cfgst_ok;
        }
    } else if (strcmp(key, APP_CFG_POSITION) == 0) {
        if (strcmp(value, APP_FROM_PARENT) == 0) {
            ctx.window.x = POS_FROM_PARENT;
            ctx.window.y = POS_FROM_PARENT;
            status = cfgst_ok;
        } else {
            struct str_slice slices[2];
            ssize_t x, y;
            if (str_split(value, ',', slices, 2) == 2 &&
                str_to_num(slices[0].value, slices[0].len, &x, 0) &&
                str_to_num(slices[1].value, slices[1].len, &y, 0)) {
                ctx.window.x = (ssize_t)x;
                ctx.window.y = (ssize_t)y;
                status = cfgst_ok;
            }
        }
    } else if (strcmp(key, APP_CFG_SIZE) == 0) {
        ssize_t width, height;
        if (strcmp(value, APP_FROM_PARENT) == 0) {
            ctx.window.width = SIZE_FROM_PARENT;
            ctx.window.height = SIZE_FROM_PARENT;
            status = cfgst_ok;
        } else if (strcmp(value, APP_FROM_IMAGE) == 0) {
            ctx.window.width = SIZE_FROM_IMAGE;
            ctx.window.height = SIZE_FROM_IMAGE;
            status = cfgst_ok;
        } else if (strcmp(value, APP_FULLSCREEN) == 0) {
            ctx.window.width = SIZE_FULLSCREEN;
            ctx.window.height = SIZE_FULLSCREEN;
            status = cfgst_ok;
        } else {
            struct str_slice slices[2];
            if (str_split(value, ',', slices, 2) == 2 &&
                str_to_num(slices[0].value, slices[0].len, &width, 0) &&
                str_to_num(slices[1].value, slices[1].len, &height, 0) &&
                width > 0 && width < 100000 && height > 0 && height < 100000) {
                ctx.window.width = width;
                ctx.window.height = height;
                status = cfgst_ok;
            }
        }
    } else if (strcmp(key, APP_CFG_SIGUSR1) == 0) {
        if (action_create(value, &ctx.sigusr1)) {
            status = cfgst_ok;
        }
    } else if (strcmp(key, APP_CFG_SIGUSR2) == 0) {
        if (action_create(value, &ctx.sigusr2)) {
            status = cfgst_ok;
        }
    } else if (strcmp(key, APP_CFG_APP_ID) == 0) {
        str_dup(value, &ctx.app_id);
        status = cfgst_ok;
    } else {
        status = cfgst_invalid_key;
    }

    return status;
}

void app_create(void)
{
    ctx.window.x = POS_FROM_PARENT;
    ctx.window.y = POS_FROM_PARENT;
    ctx.window.width = SIZE_FROM_PARENT;
    ctx.window.height = SIZE_FROM_PARENT;
    ctx.ehandler = viewer_handle;
    str_dup(APP_NAME, &ctx.app_id);

    action_create("reload", &ctx.sigusr1);
    action_create("next_file", &ctx.sigusr2);

    font_create();
    image_list_create();
    info_create();
    keybind_create();
    viewer_create();
    gallery_create();

    // register configuration loader
    config_add_loader(APP_CFG_SECTION, load_config);
}

void app_destroy(void)
{
    loader_destroy();
    gallery_destroy();
    viewer_destroy();
    ui_destroy();
    image_list_destroy();
    info_destroy();
    font_destroy();
    keybind_destroy();

    for (size_t i = 0; i < ctx.wfds_num; ++i) {
        close(ctx.wfds[i].fd);
    }
    free(ctx.wfds);

    list_for_each(ctx.events, struct event_queue, it) {
        if (it->event.type == event_load) {
            image_free(it->event.param.load.image);
        }
        free(it);
    }
    if (ctx.event_signal != -1) {
        notification_free(ctx.event_signal);
    }
    pthread_mutex_destroy(&ctx.events_lock);

    action_free(&ctx.sigusr1);
    action_free(&ctx.sigusr2);
}

bool app_init(const char** sources, size_t num)
{
    bool force_load = false;
    struct image* first_image;
    struct sigaction sigact;

    // compose image list
    if (num == 0) {
        // no input files specified, use all from the current directory
        static const char* current_dir = ".";
        sources = &current_dir;
        num = 1;
    } else if (num == 1) {
        force_load = true;
        if (strcmp(sources[0], "-") == 0) {
            // load from stdin
            static const char* stdin_name = LDRSRC_STDIN;
            sources = &stdin_name;
        }
    }
    if (image_list_init(sources, num) == 0) {
        if (force_load) {
            fprintf(stderr, "%s: Unable to open\n", sources[0]);
        } else {
            fprintf(stderr, "No image files found to view, exit\n");
        }
        return false;
    }

    // load the first image
    first_image = load_first_file(image_list_find(sources[0]), force_load);
    if (!first_image) {
        return false;
    }

    // setup window position and size
    if (ctx.window.width != SIZE_FULLSCREEN) {
        sway_setup(); // try Sway integration
    }
    if (ctx.window.width == SIZE_FULLSCREEN) {
        ui_toggle_fullscreen();
    } else if (ctx.window.width == SIZE_FROM_IMAGE ||
               ctx.window.width == SIZE_FROM_PARENT) {
        // fixup window size form the first image
        const struct pixmap* pm = &first_image->frames[0].pm;
        ctx.window.width = pm->width;
        ctx.window.height = pm->height;
    }

    // connect to wayland
    if (!ui_init(ctx.app_id, ctx.window.width, ctx.window.height)) {
        return false;
    }

    // create event queue notification
    ctx.event_signal = notification_create();
    if (ctx.event_signal != -1) {
        app_watch(ctx.event_signal, handle_event_queue, NULL);
    } else {
        perror("Unable to create eventfd");
        return false;
    }
    pthread_mutex_init(&ctx.events_lock, NULL);

    // initialize other subsystems
    font_init();
    info_init();
    loader_init();
    viewer_init(ctx.ehandler == viewer_handle ? first_image : NULL);
    gallery_init(ctx.ehandler == gallery_handle ? first_image : NULL);

    // set mode for info
    if (info_enabled()) {
        info_switch(ctx.ehandler == viewer_handle ? APP_MODE_VIEWER
                                                  : APP_MODE_GALLERY);
    }

    // set signal handler
    sigact.sa_handler = on_signal;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGUSR1, &sigact, NULL);
    sigaction(SIGUSR2, &sigact, NULL);

    return true;
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
        for (size_t i = 0; i < ctx.wfds_num; ++i) {
            if (fds[i].revents & POLLIN) {
                ctx.wfds[i].callback(ctx.wfds[i].data);
            }
        }

        ui_event_done();
    }

    free(fds);

    return ctx.state != loop_error;
}

void app_exit(int rc)
{
    ctx.state = rc ? loop_error : loop_stop;
}

void app_switch_mode(size_t index)
{
    const char* info_mode;
    const struct event event = {
        .type = event_activate,
        .param.activate.index = index,
    };

    if (ctx.ehandler == viewer_handle) {
        ctx.ehandler = gallery_handle;
        info_mode = APP_MODE_GALLERY;
    } else {
        ctx.ehandler = viewer_handle;
        info_mode = APP_MODE_VIEWER;
    }

    ctx.ehandler(&event);

    if (info_enabled()) {
        info_switch(info_mode);
    }
    if (info_help_active()) {
        info_switch_help();
    }

    app_redraw();
}

bool app_is_viewer(void)
{
    return ctx.ehandler == viewer_handle;
}

void app_reload(void)
{
    static const struct action action = { .type = action_reload };
    const struct event event = {
        .type = event_action,
        .param.action = &action,
    };
    append_event(&event);
}

void app_redraw(void)
{
    const struct event event = {
        .type = event_redraw,
    };

    // remove the same event to append new one to tail
    pthread_mutex_lock(&ctx.events_lock);
    list_for_each(ctx.events, struct event_queue, it) {
        if (it->event.type == event_redraw) {
            ctx.events = list_remove(it);
            free(it);
            break;
        }
    }
    pthread_mutex_unlock(&ctx.events_lock);

    append_event(&event);
}

void app_on_resize(void)
{
    const struct event event = {
        .type = event_resize,
    };
    append_event(&event);
}

void app_on_keyboard(xkb_keysym_t key, uint8_t mods)
{
    const struct keybind* kb = keybind_find(key, mods);

    if (kb) {
        for (size_t i = 0; i < kb->actions.num; ++i) {
            apply_action(&kb->actions.sequence[i]);
        }
    } else {
        char* name = keybind_name(key, mods);
        if (name) {
            info_update(info_status, "Key %s is not bound", name);
            free(name);
            app_redraw();
        }
    }
}

void app_on_drag(int dx, int dy)
{
    const struct event event = { .type = event_drag,
                                 .param.drag.dx = dx,
                                 .param.drag.dy = dy };

    // merge with existing event
    pthread_mutex_lock(&ctx.events_lock);
    list_for_each(ctx.events, struct event_queue, it) {
        if (it->event.type == event_drag) {
            it->event.param.drag.dx += dx;
            it->event.param.drag.dy += dy;
            pthread_mutex_unlock(&ctx.events_lock);
            return;
        }
    }
    pthread_mutex_unlock(&ctx.events_lock);

    append_event(&event);
}

void app_on_load(struct image* image, size_t index)
{
    const struct event event = {
        .type = event_load,
        .param.load.image = image,
        .param.load.index = index,
    };
    append_event(&event);
}

void app_execute(const char* expr, const char* path)
{
    char* cmd = NULL;
    int rc = -1;

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
        } else if (errno) {
            rc = errno;
        }
    }

    // show execution status
    if (!cmd) {
        info_update(info_status, "Error: no command to execute");
    } else {
        size_t max_len = 30; // trim long command text
        if (strlen(cmd) > max_len) {
            const char* ellipsis = "...";
            const size_t ellipsis_len = strlen(ellipsis);
            memcpy(&cmd[max_len - ellipsis_len], ellipsis, ellipsis_len + 1);
        }
        if (rc) {
            info_update(info_status, "Error %d: %s", rc, cmd);
        } else {
            info_update(info_status, "OK: %s", cmd);
        }
    }

    free(cmd);

    app_redraw();
}
