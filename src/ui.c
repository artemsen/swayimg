// SPDX-License-Identifier: MIT
// User interface: Window managment, keyboard input, etc.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "ui.h"

#include "buildcfg.h"
#include "xdg-shell-protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

// Max number of output displays
#define MAX_OUTPUTS 4

/** Loop state */
enum state {
    state_ok,
    state_exit,
    state_error,
};

/** UI context */
struct ui {
    // wayland specific
    struct wl {
        struct wl_display* display;
        struct wl_registry* registry;
        struct wl_shm* shm;
        struct wl_compositor* compositor;
        struct wl_seat* seat;
        struct wl_keyboard* keyboard;
        struct wl_pointer* pointer;
        struct wl_surface* surface;
        struct wl_output* output;
    } wl;

    // outputs and their scale factors
    struct outputs {
        struct wl_output* output;
        int32_t scale;
    } outputs[MAX_OUTPUTS];

    // cross-desktop
    struct xdg {
        struct xdg_wm_base* base;
        struct xdg_surface* surface;
        struct xdg_toplevel* toplevel;
    } xdg;

    // keayboard
    struct xkb {
        struct xkb_context* context;
        struct xkb_keymap* keymap;
        struct xkb_state* state;
    } xkb;

    // key repeat data
    struct repeat {
        int fd;
        xkb_keysym_t key;
        uint32_t rate;
        uint32_t delay;
    } repeat;

    // window buffer
    struct wnd {
        struct wl_buffer* buffer;
        void* data;
        size_t width;
        size_t height;
        int32_t scale;
    } wnd;

    // timers
    int timer_animation;
    int timer_slideshow;

    // event handlers
    const struct ui_handlers* handlers;

    // global state
    enum state state;
};

/**
 * Fill timespec structure.
 * @param ts destination structure
 * @param ms time in milliseconds
 */
static inline void set_timespec(struct timespec* ts, uint32_t ms)
{
    ts->tv_sec = ms / 1000;
    ts->tv_nsec = (ms % 1000) * 1000000;
}

/**
 * Create shared memory file.
 * @param sz size of data in bytes
 * @param data pointer to mapped data
 * @return shared file descriptor, -1 on errors
 */
static int create_shmem(size_t sz, void** data)
{
    char path[64];
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    snprintf(path, sizeof(path), "/" APP_NAME "_%lx",
             (ts.tv_sec << 32) | ts.tv_nsec);

    int fd = shm_open(path, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd == -1) {
        fprintf(stderr, "Unable to create shared file: [%i] %s\n", errno,
                strerror(errno));
        return -1;
    }

    shm_unlink(path);

    if (ftruncate(fd, sz) == -1) {
        fprintf(stderr, "Unable to truncate shared file: [%i] %s\n", errno,
                strerror(errno));
        close(fd);
        return -1;
    }

    *data = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        fprintf(stderr, "Unable to map shared file: [%i] %s\n", errno,
                strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

/**
 * Redraw window.
 * @param ctx window context
 */
static void redraw(struct ui* ctx)
{
    ctx->handlers->on_redraw(ctx->handlers->data, ctx->wnd.data);

    wl_surface_attach(ctx->wl.surface, ctx->wnd.buffer, 0, 0);
    wl_surface_damage(ctx->wl.surface, 0, 0, ctx->wnd.width, ctx->wnd.height);
    wl_surface_set_buffer_scale(ctx->wl.surface, ctx->wnd.scale);
    wl_surface_commit(ctx->wl.surface);
}

/**
 * (Re)create buffer.
 * @param ctx window context
 * @return true if operation completed successfully
 */
static bool create_buffer(struct ui* ctx)
{
    const size_t stride = ctx->wnd.width * sizeof(argb_t);
    const size_t buf_sz = stride * ctx->wnd.height;
    struct wl_shm_pool* pool;
    int fd;
    bool status;

    // free previous allocated buffer
    if (ctx->wnd.buffer) {
        wl_buffer_destroy(ctx->wnd.buffer);
        ctx->wnd.buffer = NULL;
    }

    // create new buffer
    fd = create_shmem(buf_sz, &ctx->wnd.data);
    status = (fd != -1);
    if (status) {
        pool = wl_shm_create_pool(ctx->wl.shm, fd, buf_sz);
        close(fd);
        ctx->wnd.buffer =
            wl_shm_pool_create_buffer(pool, 0, ctx->wnd.width, ctx->wnd.height,
                                      stride, WL_SHM_FORMAT_ARGB8888);
        wl_shm_pool_destroy(pool);
        ctx->handlers->on_resize(ctx->handlers->data, ctx, ctx->wnd.width,
                                 ctx->wnd.height, ctx->wnd.scale);
    }

    return status;
}

// suppress unused parameter warnings
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

/*******************************************************************************
 * Keyboard handlers
 ******************************************************************************/
static void on_keyboard_enter(void* data, struct wl_keyboard* wl_keyboard,
                              uint32_t serial, struct wl_surface* surface,
                              struct wl_array* keys)
{
}

static void on_keyboard_leave(void* data, struct wl_keyboard* wl_keyboard,
                              uint32_t serial, struct wl_surface* surface)
{
}

static void on_keyboard_modifiers(void* data, struct wl_keyboard* wl_keyboard,
                                  uint32_t serial, uint32_t mods_depressed,
                                  uint32_t mods_latched, uint32_t mods_locked,
                                  uint32_t group)
{
    struct ui* ctx = data;
    xkb_state_update_mask(ctx->xkb.state, mods_depressed, mods_latched,
                          mods_locked, 0, 0, group);
}

static void on_keyboard_repeat_info(void* data, struct wl_keyboard* wl_keyboard,
                                    int32_t rate, int32_t delay)
{
    struct ui* ctx = data;

    // save keyboard repeat preferences
    ctx->repeat.rate = rate;
    ctx->repeat.delay = delay;
}

static void on_keyboard_keymap(void* data, struct wl_keyboard* wl_keyboard,
                               uint32_t format, int32_t fd, uint32_t size)
{
    struct ui* ctx = data;
    char* keymap;

    xkb_state_unref(ctx->xkb.state);
    xkb_keymap_unref(ctx->xkb.keymap);

    keymap = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    ctx->xkb.keymap = xkb_keymap_new_from_string(ctx->xkb.context, keymap,
                                                 XKB_KEYMAP_FORMAT_TEXT_V1,
                                                 XKB_KEYMAP_COMPILE_NO_FLAGS);
    ctx->xkb.state = xkb_state_new(ctx->xkb.keymap);

    munmap(keymap, size);
    close(fd);
}

static void on_keyboard_key(void* data, struct wl_keyboard* wl_keyboard,
                            uint32_t serial, uint32_t time, uint32_t key,
                            uint32_t state)
{
    struct ui* ctx = data;
    struct itimerspec ts = { 0 };

    if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        // stop key repeat timer
        timerfd_settime(ctx->repeat.fd, 0, &ts, NULL);
    } else if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        xkb_keysym_t keysym;
        key += 8;
        keysym = xkb_state_key_get_one_sym(ctx->xkb.state, key);
        if (keysym != XKB_KEY_NoSymbol) {
            // handle key in viewer
            if (ctx->handlers->on_keyboard(ctx->handlers->data, ctx, keysym)) {
                redraw(ctx);
            }
            // handle key repeat
            if (ctx->repeat.rate &&
                xkb_keymap_key_repeats(ctx->xkb.keymap, key)) {
                // start key repeat timer
                ctx->repeat.key = keysym;
                set_timespec(&ts.it_value, ctx->repeat.delay);
                set_timespec(&ts.it_interval, 1000 / ctx->repeat.rate);
                timerfd_settime(ctx->repeat.fd, 0, &ts, NULL);
            }
        }
    }
}

static void on_pointer_enter(void* data, struct wl_pointer* wl_pointer,
                             uint32_t serial, struct wl_surface* surface,
                             wl_fixed_t surface_x, wl_fixed_t surface_y)
{
}

static void on_pointer_leave(void* data, struct wl_pointer* wl_pointer,
                             uint32_t serial, struct wl_surface* surface)
{
}

static void on_pointer_motion(void* data, struct wl_pointer* wl_pointer,
                              uint32_t time, wl_fixed_t surface_x,
                              wl_fixed_t surface_y)
{
}

static void on_pointer_button(void* data, struct wl_pointer* wl_pointer,
                              uint32_t serial, uint32_t time, uint32_t button,
                              uint32_t state)
{
}

static void on_pointer_axis(void* data, struct wl_pointer* wl_pointer,
                            uint32_t time, uint32_t axis, wl_fixed_t value)
{
    struct ui* ctx = data;
    xkb_keysym_t key = value > 0 ? XKB_KEY_SunPageDown : XKB_KEY_SunPageUp;

    if (ctx->handlers->on_keyboard(ctx->handlers->data, ctx, key)) {
        redraw(ctx);
    }
}

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = on_keyboard_keymap,
    .enter = on_keyboard_enter,
    .leave = on_keyboard_leave,
    .key = on_keyboard_key,
    .modifiers = on_keyboard_modifiers,
    .repeat_info = on_keyboard_repeat_info,
};

static const struct wl_pointer_listener pointer_listener = {
    .enter = on_pointer_enter,
    .leave = on_pointer_leave,
    .motion = on_pointer_motion,
    .button = on_pointer_button,
    .axis = on_pointer_axis,
};

/*******************************************************************************
 * Seat handlers
 ******************************************************************************/
static void on_seat_name(void* data, struct wl_seat* seat, const char* name) { }

static void on_seat_capabilities(void* data, struct wl_seat* seat, uint32_t cap)
{
    struct ui* ctx = data;

    if (cap & WL_SEAT_CAPABILITY_KEYBOARD) {
        ctx->wl.keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(ctx->wl.keyboard, &keyboard_listener, ctx);
    } else if (ctx->wl.keyboard) {
        wl_keyboard_destroy(ctx->wl.keyboard);
        ctx->wl.keyboard = NULL;
    }

    if (cap & WL_SEAT_CAPABILITY_POINTER) {
        ctx->wl.pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(ctx->wl.pointer, &pointer_listener, ctx);
    } else if (ctx->wl.pointer) {
        wl_pointer_destroy(ctx->wl.pointer);
        ctx->wl.pointer = NULL;
    }
}

static const struct wl_seat_listener seat_listener = { .capabilities =
                                                           on_seat_capabilities,
                                                       .name = on_seat_name };

/*******************************************************************************
 * XDG handlers
 ******************************************************************************/
static void on_xdg_surface_configure(void* data, struct xdg_surface* surface,
                                     uint32_t serial)
{
    struct ui* ctx = data;

    xdg_surface_ack_configure(surface, serial);

    if (!ctx->wnd.buffer && !create_buffer(ctx)) {
        ctx->state = state_error;
        return;
    }

    redraw(ctx);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = on_xdg_surface_configure
};

static void on_xdg_ping(void* data, struct xdg_wm_base* base, uint32_t serial)
{
    xdg_wm_base_pong(base, serial);
}

static const struct xdg_wm_base_listener xdg_base_listener = {
    .ping = on_xdg_ping
};

static void handle_xdg_toplevel_configure(void* data, struct xdg_toplevel* lvl,
                                          int32_t width, int32_t height,
                                          struct wl_array* states)
{
    struct ui* ctx = data;
    const int32_t cur_width = (int32_t)(ctx->wnd.width / ctx->wnd.scale);
    const int32_t cur_height = (int32_t)(ctx->wnd.height / ctx->wnd.scale);

    if (width && height && (width != cur_width || height != cur_height)) {
        ctx->wnd.width = width * ctx->wnd.scale;
        ctx->wnd.height = height * ctx->wnd.scale;
        if (!create_buffer(ctx)) {
            ctx->state = state_error;
        }
    }
}

static void handle_xdg_toplevel_close(void* data, struct xdg_toplevel* top)
{
    struct ui* ctx = data;
    ctx->state = state_exit;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = handle_xdg_toplevel_configure,
    .close = handle_xdg_toplevel_close,
};

/*******************************************************************************
 * WL Output handlers
 ******************************************************************************/
static void on_output_geometry(void* data, struct wl_output* output, int32_t x,
                               int32_t y, int32_t physical_width,
                               int32_t physical_height, int32_t subpixel,
                               const char* make, const char* model,
                               int32_t transform)
{
}

static void on_output_mode(void* data, struct wl_output* output, uint32_t flags,
                           int32_t width, int32_t height, int32_t refresh)
{
}

static void on_output_done(void* data, struct wl_output* output) { }

static void on_output_scale(void* data, struct wl_output* output,
                            int32_t factor)
{
    struct ui* ctx = data;

    // save output scale factor
    for (size_t i = 0; i < MAX_OUTPUTS; ++i) {
        if (!ctx->outputs[i].output || ctx->outputs[i].output == output) {
            ctx->outputs[i].output = output;
            ctx->outputs[i].scale = factor;
            break;
        }
    }
}

static const struct wl_output_listener wl_output_listener = {
    .geometry = on_output_geometry,
    .mode = on_output_mode,
    .done = on_output_done,
    .scale = on_output_scale,
};

static void handle_enter_surface(void* data, struct wl_surface* surface,
                                 struct wl_output* output)
{
    struct ui* ctx = data;
    int32_t scale = 1;

    // find scale factor for current output
    for (size_t i = 0; i < MAX_OUTPUTS; ++i) {
        if (ctx->outputs[i].output == output) {
            scale = ctx->outputs[i].scale;
            break;
        }
    }

    // recreate buffer if scale has changed
    if (scale != ctx->wnd.scale) {
        ctx->wnd.width = (ctx->wnd.width / ctx->wnd.scale) * scale;
        ctx->wnd.height = (ctx->wnd.height / ctx->wnd.scale) * scale;
        ctx->wnd.scale = scale;
        if (create_buffer(ctx)) {
            redraw(ctx);
        } else {
            ctx->state = state_error;
        }
    }
}

static void handle_leave_surface(void* data, struct wl_surface* surface,
                                 struct wl_output* output)
{
}

static const struct wl_surface_listener wl_surface_listener = {
    .enter = handle_enter_surface,
    .leave = handle_leave_surface,
};

/*******************************************************************************
 * Registry handlers
 ******************************************************************************/
static void on_registry_global(void* data, struct wl_registry* registry,
                               uint32_t name, const char* interface,
                               uint32_t version)
{
    struct ui* ctx = data;

    if (strcmp(interface, wl_shm_interface.name) == 0) {
        ctx->wl.shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, wl_compositor_interface.name) == 0) {
        ctx->wl.compositor =
            wl_registry_bind(registry, name, &wl_compositor_interface, 3);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        if (ctx->wl.output) {
            wl_output_release(ctx->wl.output);
        }
        ctx->wl.output =
            wl_registry_bind(registry, name, &wl_output_interface, 3);
        wl_output_add_listener(ctx->wl.output, &wl_output_listener, data);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        ctx->xdg.base =
            wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(ctx->xdg.base, &xdg_base_listener, data);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        ctx->wl.seat = wl_registry_bind(registry, name, &wl_seat_interface, 4);
        wl_seat_add_listener(ctx->wl.seat, &seat_listener, data);
    }
}

void on_registry_remove(void* data, struct wl_registry* registry, uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
    .global = on_registry_global, .global_remove = on_registry_remove
};

#pragma GCC diagnostic pop // "-Wunused-parameter"

struct ui* ui_create(const struct config* cfg,
                     const struct ui_handlers* handlers)
{
    struct ui* ctx;

    ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        fprintf(stderr, "Not enough memory\n");
        return NULL;
    }
    ctx->wnd.scale = 1;
    ctx->repeat.fd = -1;
    ctx->timer_animation = -1;
    ctx->timer_slideshow = -1;
    ctx->wnd.width = cfg->geometry.width ? cfg->geometry.width : 800;
    ctx->wnd.height = cfg->geometry.height ? cfg->geometry.height : 600;
    ctx->handlers = handlers;

    ctx->wl.display = wl_display_connect(NULL);
    if (!ctx->wl.display) {
        fprintf(stderr, "Failed to open display\n");
        return NULL;
    }
    ctx->xkb.context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    ctx->wl.registry = wl_display_get_registry(ctx->wl.display);
    if (!ctx->wl.registry) {
        fprintf(stderr, "Failed to open registry\n");
        ui_free(ctx);
        return NULL;
    }
    wl_registry_add_listener(ctx->wl.registry, &registry_listener, ctx);
    wl_display_roundtrip(ctx->wl.display);

    ctx->wl.surface = wl_compositor_create_surface(ctx->wl.compositor);
    if (!ctx->wl.surface) {
        fprintf(stderr, "Failed to create surface\n");
        ui_free(ctx);
        return NULL;
    }
    wl_surface_add_listener(ctx->wl.surface, &wl_surface_listener, ctx);

    ctx->xdg.surface =
        xdg_wm_base_get_xdg_surface(ctx->xdg.base, ctx->wl.surface);
    if (!ctx->xdg.surface) {
        fprintf(stderr, "Failed to create xdg surface\n");
        ui_free(ctx);
        return NULL;
    }
    xdg_surface_add_listener(ctx->xdg.surface, &xdg_surface_listener, ctx);
    ctx->xdg.toplevel = xdg_surface_get_toplevel(ctx->xdg.surface);
    xdg_toplevel_add_listener(ctx->xdg.toplevel, &xdg_toplevel_listener, ctx);
    xdg_toplevel_set_app_id(ctx->xdg.toplevel, cfg->app_id);
    if (cfg->fullscreen) {
        xdg_toplevel_set_fullscreen(ctx->xdg.toplevel, NULL);
    }

    wl_surface_commit(ctx->wl.surface);

    ctx->repeat.fd =
        timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    ctx->timer_animation =
        timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    ctx->timer_slideshow =
        timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);

    return ctx;
}

void ui_free(struct ui* ctx)
{
    if (!ctx) {
        return;
    }
    if (ctx->repeat.fd != -1) {
        close(ctx->repeat.fd);
    }
    if (ctx->timer_animation != -1) {
        close(ctx->timer_animation);
    }
    if (ctx->timer_slideshow != -1) {
        close(ctx->timer_slideshow);
    }
    if (ctx->xkb.state) {
        xkb_state_unref(ctx->xkb.state);
    }
    if (ctx->xkb.keymap) {
        xkb_keymap_unref(ctx->xkb.keymap);
    }
    if (ctx->xkb.context) {
        xkb_context_unref(ctx->xkb.context);
    }
    if (ctx->wnd.buffer) {
        wl_buffer_destroy(ctx->wnd.buffer);
    }
    if (ctx->wl.seat) {
        wl_seat_destroy(ctx->wl.seat);
    }
    if (ctx->wl.keyboard) {
        wl_keyboard_destroy(ctx->wl.keyboard);
    }
    if (ctx->wl.pointer) {
        wl_pointer_destroy(ctx->wl.pointer);
    }
    if (ctx->wl.shm) {
        wl_shm_destroy(ctx->wl.shm);
    }
    if (ctx->xdg.toplevel) {
        xdg_toplevel_destroy(ctx->xdg.toplevel);
    }
    if (ctx->xdg.base) {
        xdg_surface_destroy(ctx->xdg.surface);
    }
    if (ctx->xdg.base) {
        xdg_wm_base_destroy(ctx->xdg.base);
    }
    if (ctx->wl.output) {
        wl_output_destroy(ctx->wl.output);
    }
    if (ctx->wl.surface) {
        wl_surface_destroy(ctx->wl.surface);
    }
    if (ctx->wl.compositor) {
        wl_compositor_destroy(ctx->wl.compositor);
    }
    if (ctx->wl.registry) {
        wl_registry_destroy(ctx->wl.registry);
    }
    wl_display_disconnect(ctx->wl.display);

    free(ctx);
}

bool ui_run(struct ui* ctx)
{
    // file descriptors to poll
    struct pollfd fds[] = {
        { .fd = wl_display_get_fd(ctx->wl.display), .events = POLLIN },
        { .fd = ctx->repeat.fd, .events = POLLIN },
        { .fd = ctx->timer_animation, .events = POLLIN },
        { .fd = ctx->timer_slideshow, .events = POLLIN },
    };

    while (ctx->state == state_ok) {
        // prepare to read wayland events
        while (wl_display_prepare_read(ctx->wl.display) != 0) {
            wl_display_dispatch_pending(ctx->wl.display);
        }
        wl_display_flush(ctx->wl.display);

        // poll events
        if (poll(fds, sizeof(fds) / sizeof(fds[0]), -1) <= 0) {
            wl_display_cancel_read(ctx->wl.display);
            continue;
        }

        // read and handle wayland events
        if (fds[0].revents & POLLIN) {
            wl_display_read_events(ctx->wl.display);
            wl_display_dispatch_pending(ctx->wl.display);
        } else {
            wl_display_cancel_read(ctx->wl.display);
        }

        // read and handle key repeat events from timer
        if (fds[1].revents & POLLIN) {
            uint64_t repeats;
            if (read(ctx->repeat.fd, &repeats, sizeof(repeats)) ==
                sizeof(repeats)) {
                bool handled = false;
                while (repeats--) {
                    handled |= ctx->handlers->on_keyboard(ctx->handlers->data,
                                                          ctx, ctx->repeat.key);
                }
                if (handled) {
                    redraw(ctx);
                }
            }
        }

        // animation timer
        if (fds[2].revents & POLLIN) {
            const struct itimerspec ts = { 0 };
            timerfd_settime(ctx->timer_animation, 0, &ts, NULL);
            ctx->handlers->on_timer(ctx->handlers->data, ui_timer_animation,
                                    ctx);
            redraw(ctx);
        }
        // slideshow timer
        if (fds[3].revents & POLLIN) {
            const struct itimerspec ts = { 0 };
            timerfd_settime(ctx->timer_slideshow, 0, &ts, NULL);
            ctx->handlers->on_timer(ctx->handlers->data, ui_timer_slideshow,
                                    ctx);
            redraw(ctx);
        }
    }

    return ctx->state != state_error;
}

void ui_stop(struct ui* ctx)
{
    ctx->state = state_exit;
}

void ui_set_title(struct ui* ctx, const char* title)
{
    xdg_toplevel_set_title(ctx->xdg.toplevel, title);
}

void ui_set_fullscreen(struct ui* ctx, bool enable)
{
    if (enable) {
        xdg_toplevel_set_fullscreen(ctx->xdg.toplevel, NULL);
    } else {
        xdg_toplevel_unset_fullscreen(ctx->xdg.toplevel);
    }
}

void ui_set_timer(struct ui* ctx, enum ui_timer timer, size_t ms)
{
    int fd;
    struct itimerspec ts = { 0 };

    switch (timer) {
        case ui_timer_animation:
            fd = ctx->timer_animation;
            break;
        case ui_timer_slideshow:
            fd = ctx->timer_slideshow;
            break;
        default:
            return;
    }

    set_timespec(&ts.it_value, ms);
    timerfd_settime(fd, 0, &ts, NULL);
}
