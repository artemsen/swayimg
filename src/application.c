// SPDX-License-Identifier: MIT
// Image viewer application: main loop and event handler.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "application.h"

#include "array.h"
#include "buildcfg.h"
#include "font.h"
#include "gallery.h"
#include "imglist.h"
#include "info.h"
#include "shellcmd.h"
#include "sway.h"
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

/** Event types. */
enum event_type {
    event_action, ///< Apply action
    event_redraw, ///< Redraw window request
    event_drag,   ///< Mouse or touch drag operation
};

/* Event queue. */
struct event_queue {
    struct list list;     ///< Links to prev/next entry
    enum event_type type; ///< Event type
    union event_params {  ///< Event parameters
        const struct action* action;
        struct drag {
            int dx;
            int dy;
        } drag;
    } param;
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

    enum mode_type mode_current; ///< Currently active mode (viewer/gallery)
    struct mode_handlers mode_handlers[2]; ///< Mode handlers

    struct wndrect window; ///< Preferable window position and size
    bool wnd_decor;        ///< Window decoration: borders and title
    char* app_id;          ///< Application id (app_id name)
};

/** Global application context. */
static struct application ctx;

#ifdef HAVE_SWAYWM
/**
 * Setup window position via Sway IPC.
 * @param cfg config instance
 */
static void sway_setup(const struct config* cfg)
{
    struct wndrect parent;
    bool fullscreen;
    int border;
    int ipc;
    const bool abs_coordinates = (ctx.window.x != POS_FROM_PARENT);

    ipc = sway_connect();
    if (ipc == INVALID_SWAY_IPC) {
        return; // sway not available
    }
    if (!sway_current(ipc, &parent, &border, &fullscreen)) {
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
        if (!abs_coordinates &&
            config_get_bool(cfg, CFG_GENERAL, CFG_GNRL_DECOR)) {
            ctx.window.width -= border * 2;
            ctx.window.height -= border * 2;
        }
    }
    if (ctx.window.x == POS_FROM_PARENT) {
        ctx.window.x = parent.x;
        ctx.window.y = parent.y;
    }

    // set window position via sway rules
    sway_add_rules(ipc, ctx.window.x, ctx.window.y, abs_coordinates);

    sway_disconnect(ipc);
}
#endif // HAVE_SWAYWM

/** Switch mode (viewer/gallery). */
static void switch_mode(void)
{
    struct image* current = ctx.mode_handlers[ctx.mode_current].deactivate();

    if (ctx.mode_current == mode_viewer) {
        ctx.mode_current = mode_gallery;
    } else {
        ctx.mode_current = mode_viewer;
    }

    ctx.mode_handlers[ctx.mode_current].activate(current);

    if (info_enabled()) {
        info_switch(ctx.mode_current == mode_viewer ? CFG_MODE_VIEWER
                                                    : CFG_MODE_GALLERY);
    }
    if (info_help_active()) {
        info_switch_help();
    }

    app_redraw();
}

/**
 * Execute system command for the specified image.
 * @param expr command expression
 * @param path file path to substitute into expression
 */
static void execute_cmd(const char* expr, const char* path)
{
    const size_t max_status = 60;
    struct array* out = NULL;
    struct array* err = NULL;
    char* msg = NULL;
    char* cmd;
    int rc;

    // contruct and execute command
    cmd = shellcmd_expr(expr, path);
    if (!cmd) {
        info_update(info_status, "Error: no command to execute");
        app_redraw();
        return;
    }
    rc = shellcmd_exec(cmd, &out, &err);

    // duplicate output to stdout/stderr
    if (out) {
        fprintf(stdout, "%.*s", (int)out->size, out->data);
    }
    if (err) {
        fprintf(stderr, "%.*s", (int)err->size, err->data);
    }

    // show execution status
    if (rc == 0) {
        if (out) {
            str_append((const char*)out->data, out->size, &msg);
        } else {
            str_dup("Success: ", &msg);
            str_append(cmd, 0, &msg);
        }
    } else if (rc == SHELLCMD_TIMEOUT) {
        str_dup("Child process timed out: ", &msg);
        str_append(cmd, 0, &msg);
    } else {
        char desc[256];
        snprintf(desc, sizeof(desc), "Error %d: ", rc);
        str_dup(desc, &msg);
        if (err) {
            str_append((const char*)err->data, err->size, &msg);
        } else if (out) {
            str_append((const char*)out->data, out->size, &msg);
        } else {
            str_append(strerror(rc), 0, &msg);
        }
    }
    if (strlen(msg) > max_status) {
        // trim long output text
        const char ellipsis[] = "...";
        memcpy(msg + max_status - sizeof(ellipsis), ellipsis, sizeof(ellipsis));
    }

    info_update(info_status, "%s", msg);

    free(cmd);
    free(msg);
    arr_free(out);
    arr_free(err);

    app_redraw();
}

/** Notification callback: handle event queue. */
static void handle_event_queue(__attribute__((unused)) void* data)
{
    // reset notification
    uint64_t value;
    ssize_t len;
    do {
        len = read(ctx.event_signal, &value, sizeof(value));
    } while (len == -1 && errno == EINTR);

    while (ctx.events && ctx.state == loop_run) {
        struct event_queue* entry = NULL;
        pthread_mutex_lock(&ctx.events_lock);
        if (ctx.events) {
            entry = ctx.events;
            ctx.events = list_remove(entry);
        }
        pthread_mutex_unlock(&ctx.events_lock);
        if (!entry) {
            continue;
        }

        switch (entry->type) {
            case event_action: {
                const struct action* action = entry->param.action;
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
                    case action_mode:
                        switch_mode();
                        break;
                    case action_exec:
                        execute_cmd(action->params,
                                    ctx.mode_handlers[ctx.mode_current]
                                        .current()
                                        ->source);
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
                        ctx.mode_handlers[ctx.mode_current].action(action);
                        break;
                }
            } break;
            case event_redraw: {
                struct pixmap* window = ui_draw_begin();
                if (window) {
                    ctx.mode_handlers[ctx.mode_current].redraw(window);
                    ui_draw_commit();
                }
            } break;
            case event_drag:
                if (ctx.mode_handlers[ctx.mode_current].drag) {
                    ctx.mode_handlers[ctx.mode_current].drag(
                        entry->param.drag.dx, entry->param.drag.dy);
                }
                break;
        }

        free(entry);
    }
}

/**
 * Append event to queue.
 * @param evt event type
 * @param evp event parameters
 */
static void append_event(enum event_type evt, const union event_params* evp)
{
    struct event_queue* entry;
    const uint64_t value = 1;
    ssize_t len;

    // create new entry
    entry = calloc(1, sizeof(*entry));
    if (!entry) {
        return;
    }
    entry->type = evt;
    if (evp) {
        memcpy(&entry->param, evp, sizeof(*evp));
    }

    // add to queue tail
    pthread_mutex_lock(&ctx.events_lock);
    ctx.events = list_append(ctx.events, entry);
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
        const union event_params evp = { .action = &sigact->sequence[i] };
        append_event(event_action, &evp);
    }
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
 * Load config.
 * @param cfg config instance
 */
static void load_config(const struct config* cfg)
{
    const char* value;

    // startup mode
    static const char* modes[] = { CFG_MODE_VIEWER, CFG_MODE_GALLERY };
    if (config_get_oneof(cfg, CFG_GENERAL, CFG_GNRL_MODE, modes,
                         ARRAY_SIZE(modes)) == 1) {
        ctx.mode_current = mode_gallery;
    } else {
        ctx.mode_current = mode_viewer;
    }

    // initial window position
    ctx.window.x = POS_FROM_PARENT;
    ctx.window.y = POS_FROM_PARENT;
    value = config_get(cfg, CFG_GENERAL, CFG_GNRL_POSITION);
    if (strcmp(value, CFG_FROM_PARENT) != 0) {
        struct str_slice slices[2];
        ssize_t x, y;
        if (str_split(value, ',', slices, 2) == 2 &&
            str_to_num(slices[0].value, slices[0].len, &x, 0) &&
            str_to_num(slices[1].value, slices[1].len, &y, 0)) {
            ctx.window.x = (ssize_t)x;
            ctx.window.y = (ssize_t)y;
        } else {
            config_error_val(CFG_GENERAL, CFG_GNRL_POSITION);
        }
    }

    // initial window size
    value = config_get(cfg, CFG_GENERAL, CFG_GNRL_SIZE);
    if (strcmp(value, CFG_FROM_PARENT) == 0) {
        ctx.window.width = SIZE_FROM_PARENT;
        ctx.window.height = SIZE_FROM_PARENT;
    } else if (strcmp(value, CFG_FROM_IMAGE) == 0) {
        ctx.window.width = SIZE_FROM_IMAGE;
        ctx.window.height = SIZE_FROM_IMAGE;
    } else if (strcmp(value, CFG_FULLSCREEN) == 0) {
        ctx.window.width = SIZE_FULLSCREEN;
        ctx.window.height = SIZE_FULLSCREEN;
    } else {
        ssize_t width, height;
        struct str_slice slices[2];
        if (str_split(value, ',', slices, 2) == 2 &&
            str_to_num(slices[0].value, slices[0].len, &width, 0) &&
            str_to_num(slices[1].value, slices[1].len, &height, 0) &&
            width > 0 && width < 100000 && height > 0 && height < 100000) {
            ctx.window.width = width;
            ctx.window.height = height;
        } else {
            ctx.window.width = SIZE_FROM_PARENT;
            ctx.window.height = SIZE_FROM_PARENT;
            config_error_val(CFG_GENERAL, CFG_GNRL_SIZE);
        }
    }

    ctx.wnd_decor = config_get_bool(cfg, CFG_GENERAL, CFG_GNRL_DECOR);

    // signal actions
    value = config_get(cfg, CFG_GENERAL, CFG_GNRL_SIGUSR1);
    if (!action_create(value, &ctx.sigusr1)) {
        config_error_val(CFG_GENERAL, CFG_GNRL_SIGUSR1);
        value = config_get_default(CFG_GENERAL, CFG_GNRL_SIGUSR1);
        action_create(value, &ctx.sigusr1);
    }
    value = config_get(cfg, CFG_GENERAL, CFG_GNRL_SIGUSR2);
    if (!action_create(value, &ctx.sigusr2)) {
        config_error_val(CFG_GENERAL, CFG_GNRL_SIGUSR2);
        value = config_get_default(CFG_GENERAL, CFG_GNRL_SIGUSR2);
        action_create(value, &ctx.sigusr2);
    }

    // app id
    value = config_get(cfg, CFG_GENERAL, CFG_GNRL_APP_ID);
    if (!*value) {
        config_error_val(CFG_GENERAL, CFG_GNRL_APP_ID);
        value = config_get_default(CFG_GENERAL, CFG_GNRL_APP_ID);
    }
    str_dup(value, &ctx.app_id);
}

bool app_init(const struct config* cfg, const char* const* sources, size_t num)
{
    struct image* first_image;
    struct sigaction sigact;

    load_config(cfg);
    imglist_init(cfg);

    first_image = create_imglist(sources, num);
    if (!first_image) {
        imglist_destroy();
        return false;
    }

    // setup window position and size
#ifdef HAVE_SWAYWM
    if (ctx.window.width != SIZE_FULLSCREEN) {
        sway_setup(cfg);
    }
#endif // HAVE_SWAYWM
    if (ctx.window.width == SIZE_FULLSCREEN) {
        ui_toggle_fullscreen();
    } else if (ctx.window.width == SIZE_FROM_IMAGE ||
               ctx.window.width == SIZE_FROM_PARENT) {
        // determine window sipize from the first frame of the first image
        struct imgframe* frame = arr_nth(first_image->data->frames, 0);
        ctx.window.width = frame->pm.width;
        ctx.window.height = frame->pm.height;
    }

    // user interface initialization
    if (!ui_init(ctx.app_id, ctx.window.width, ctx.window.height,
                 ctx.wnd_decor)) {
        imglist_destroy();
        return false;
    }

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
    font_init(cfg);
    keybind_init(cfg);
    info_init(cfg);
    viewer_init(cfg, &ctx.mode_handlers[mode_viewer]);
    gallery_init(cfg, &ctx.mode_handlers[mode_gallery]);

    // set mode for text info
    if (info_enabled()) {
        info_switch(ctx.mode_current == mode_viewer ? CFG_MODE_VIEWER
                                                    : CFG_MODE_GALLERY);
    }

    // set signal handler
    sigact.sa_handler = on_signal;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGUSR1, &sigact, NULL);
    sigaction(SIGUSR2, &sigact, NULL);

    ctx.mode_handlers[ctx.mode_current].activate(first_image);

    return true;
}

void app_destroy(void)
{
    gallery_destroy();
    viewer_destroy();
    ui_destroy();
    imglist_destroy();
    info_destroy();
    keybind_destroy();
    font_destroy();

    for (size_t i = 0; i < ctx.wfds_num; ++i) {
        close(ctx.wfds[i].fd);
    }
    free(ctx.wfds);

    list_for_each(ctx.events, struct event_queue, it) {
        free(it);
    }

    if (ctx.event_signal != -1) {
        close(ctx.event_signal);
    }
    pthread_mutex_destroy(&ctx.events_lock);

    action_free(&ctx.sigusr1);
    action_free(&ctx.sigusr2);
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

    free(fds);

    return ctx.state != loop_error;
}

void app_exit(int rc)
{
    ctx.state = rc ? loop_error : loop_stop;
}

bool app_is_viewer(void)
{
    return ctx.mode_current == mode_viewer;
}

void app_reload(void)
{
    static const struct action action = { .type = action_reload };
    const union event_params evp = { .action = &action };
    append_event(event_action, &evp);
}

void app_redraw(void)
{
    // remove the same event to append new one to tail
    pthread_mutex_lock(&ctx.events_lock);
    list_for_each(ctx.events, struct event_queue, it) {
        if (it->type == event_redraw) {
            if (list_is_last(it)) {
                pthread_mutex_unlock(&ctx.events_lock);
                return;
            }
            ctx.events = list_remove(it);
            free(it);
            break;
        }
    }
    pthread_mutex_unlock(&ctx.events_lock);

    append_event(event_redraw, NULL);
}

void app_on_imglist(const struct image* image, enum fsevent event)
{
    ctx.mode_handlers[ctx.mode_current].imglist(image, event);
}

void app_on_resize(void)
{
    ctx.mode_handlers[ctx.mode_current].resize();
}

void app_on_keyboard(xkb_keysym_t key, uint8_t mods)
{
    const struct keybind* kb = keybind_find(key, mods);

    if (kb) {
        for (size_t i = 0; i < kb->actions.num; ++i) {
            const union event_params evp = {
                .action = &kb->actions.sequence[i],
            };
            append_event(event_action, &evp);
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
    union event_params evp;

    // try to merge with existing event
    pthread_mutex_lock(&ctx.events_lock);
    list_for_each(ctx.events, struct event_queue, it) {
        if (it->type == event_drag) {
            it->param.drag.dx += dx;
            it->param.drag.dy += dy;
            pthread_mutex_unlock(&ctx.events_lock);
            return;
        }
    }
    pthread_mutex_unlock(&ctx.events_lock);

    evp.drag.dx = dx;
    evp.drag.dy = dy;
    append_event(event_drag, &evp);
}
