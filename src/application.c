// SPDX-License-Identifier: MIT
// Imag viewer application: main loop and event handler.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "application.h"

#include "font.h"
#include "imagelist.h"
#include "info.h"
#include "loader.h"
#include "sway.h"
#include "text.h"
#include "ui.h"
#include "viewer.h"

#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <unistd.h>

/** Main loop state */
enum loop_state {
    loop_run,
    loop_stop,
    loop_error,
};

/** File descriptor and its handler. */
struct watchfd {
    int fd;
    fd_callback callback;
};

/* Application event queue (list). */
struct event_entry {
    struct event event;
    struct event_entry* next;
};

/** Application context */
struct application {
    enum loop_state state; ///< Main loop state

    struct watchfd* wfds; ///< FD polling descriptors
    size_t wfds_num;      ///< Number of polling FD

    struct event_entry* events; ///< Event queue
    int event_fd;               ///< Queue change notification
};

static struct application ctx = {
    .state = loop_run,
    .event_fd = -1,
};

/**
 * Setup window position via Sway IPC.
 */
static void sway_setup(void)
{
    struct rect parent;
    bool fullscreen;
    bool absolute;
    int ipc;

    ipc = sway_connect();
    if (ipc == INVALID_SWAY_IPC) {
        return;
    }

    absolute = ui_get_x() != POS_FROM_PARENT && ui_get_y() != POS_FROM_PARENT;

    if (sway_current(ipc, &parent, &fullscreen)) {
        if (fullscreen && !ui_get_fullscreen()) {
            // force set full screen mode if current window in it
            ui_toggle_fullscreen();
        }

        // set window position and size from the parent one
        if (!absolute) {
            ui_set_position(parent.x, parent.y);
        }
        if (ui_get_width() == SIZE_FROM_PARENT ||
            ui_get_height() == SIZE_FROM_PARENT) {
            ui_set_size(parent.width, parent.height);
        }
    }

    if (!ui_get_fullscreen()) {
        sway_add_rules(ipc, ui_get_appid(), ui_get_x(), ui_get_y(), absolute);
    }

    sway_disconnect(ipc);
}

static void handle_event_queue(void)
{
    // drain the notify pipe
    uint64_t value;
    read(ctx.event_fd, &value, sizeof(value));

    // handle events from queue
    while (ctx.events) {
        struct event_entry* entry = ctx.events;
        ctx.events = ctx.events->next;
        viewer_handle(&entry->event);
        free(entry);
    }
}

static void append_event(const struct event* event)
{
    struct event_entry* entry;

    // create new entry
    entry = malloc(sizeof(*entry));
    if (!entry) {
        return;
    }
    memcpy(&entry->event, event, sizeof(entry->event));
    entry->next = NULL;

    // add to queue tail
    if (ctx.events) {
        struct event_entry* last = ctx.events;
        while (last->next) {
            last = last->next;
        }
        last->next = entry;
    } else {
        ctx.events = entry;
    }

    // raise notify fd
    const uint64_t value = 1;
    write(ctx.event_fd, &value, sizeof(value));
}

void app_create(void)
{
    font_create();
    image_list_create();
    info_create();
    keybind_create();
    loader_create();
    text_create();
    ui_create();
    viewer_create();

    // event queue notification
    ctx.event_fd = eventfd(0, 0);
    if (ctx.event_fd == -1) {
        perror("Unable to create eventfd");
    } else {
        app_watch(ctx.event_fd, handle_event_queue);
    }
}

void app_destroy(void)
{
    viewer_destroy();
    loader_destroy();
    ui_destroy();
    image_list_destroy();
    info_destroy();
    font_destroy();
    keybind_destroy();

    for (size_t i = 0; i < ctx.wfds_num; ++i) {
        close(ctx.wfds[i].fd);
    }
    free(ctx.wfds);

    while (ctx.events) {
        struct event_entry* entry = ctx.events;
        ctx.events = ctx.events->next;
        free(entry);
    }
    if (ctx.event_fd != -1) {
        close(ctx.event_fd);
    }
}

bool app_init(const char** sources, size_t num)
{
    size_t start_idx = IMGLIST_INVALID;

    // compose image list
    if (num == 0) {
        // no input files specified, use all from the current directory
        static const char* current_dir = ".";
        sources = &current_dir;
        num = 1;
    } else if (num == 1 && strcmp(sources[0], "-") == 0) {
        // load from stdin
        static const char* stdin_name = LDRSRC_STDIN;
        sources = &stdin_name;
    }
    if (image_list_init(sources, num) == 0) {
        fprintf(stderr, "No images to view, exit\n");
        return false;
    }

    // load first image
    start_idx = image_list_find(sources[0]);
    if (!loader_init(start_idx, num == 1)) {
        return false;
    }

    // setup window position and size
    if (!ui_get_fullscreen()) {
        sway_setup();
    }

    // fixup window size form the first image
    if (ui_get_width() == SIZE_FROM_IMAGE ||
        ui_get_height() == SIZE_FROM_IMAGE ||
        ui_get_width() == SIZE_FROM_PARENT ||
        ui_get_height() == SIZE_FROM_PARENT) {
        const struct pixmap* pm = &loader_current_image()->frames[0].pm;
        ui_set_size(pm->width, pm->height);
    }

    font_init();
    info_init();
    viewer_init();

    if (!ui_init()) {
        return false;
    }

    return true;
}

void app_watch(int fd, fd_callback cb)
{
    const size_t sz = (ctx.wfds_num + 1) * sizeof(*ctx.wfds);
    struct watchfd* handlers = realloc(ctx.wfds, sz);
    if (handlers) {
        ctx.wfds = handlers;
        ctx.wfds[ctx.wfds_num].fd = fd;
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
    while (ctx.state == loop_run) {
        ui_event_prepare();

        // poll events
        if (poll(fds, ctx.wfds_num, -1) <= 0) {
            perror("Error polling events");
            ctx.state = loop_error;
            break;
        }

        // call handlers for each active event
        for (size_t i = 0; i < ctx.wfds_num; ++i) {
            if (fds[i].revents & POLLIN) {
                ctx.wfds[i].callback();
            }
        }

        ui_event_done();
    }

    free(fds);

    return ctx.state != loop_error;
}

void app_on_reload(void)
{
    const struct event event = {
        .type = event_reload,
    };
    append_event(&event);
}

void app_on_redraw(void)
{
    const struct event event = {
        .type = event_redraw,
    };
    struct event_entry* prev = NULL;
    struct event_entry* it = ctx.events;

    // remove the same event to append the new one to tail
    while (it) {
        struct event_entry* next = it->next;
        if (it->event.type == event_redraw) {
            if (prev) {
                prev->next = next;
            } else {
                ctx.events = next;
            }
            free(it);
            break;
        }
        it = next;
    }

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
    const struct event event = {
        .type = event_keypress,
        .param.keypress.key = key,
        .param.keypress.mods = mods,
    };
    append_event(&event);
}

void app_on_drag(int dx, int dy)
{
    const struct event event = { .type = event_drag,
                                 .param.drag.dx = dx,
                                 .param.drag.dy = dy };
    struct event_entry* it = ctx.events;

    // merge with existing event
    while (it) {
        if (it->event.type == event_drag) {
            it->event.param.drag.dx += dx;
            it->event.param.drag.dy += dy;
            return;
        }
        it = it->next;
    }

    append_event(&event);
}

void app_on_exit(int rc)
{
    ctx.state = rc ? loop_error : loop_stop;
}
