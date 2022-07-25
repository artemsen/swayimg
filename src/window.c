// SPDX-License-Identifier: MIT
// Wayland window.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "window.h"

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

/** Window context */
struct context {
    struct wl {
        struct wl_display* display;
        struct wl_registry* registry;
        struct wl_shm* shm;
        struct wl_compositor* compositor;
        struct wl_seat* seat;
        struct wl_keyboard* keyboard;
        struct wl_surface* surface;
    } wl;

    struct xdg {
        struct xdg_wm_base* base;
        struct xdg_surface* surface;
        struct xdg_toplevel* toplevel;
    } xdg;

    struct xkb {
        struct xkb_context* context;
        struct xkb_keymap* keymap;
        struct xkb_state* state;
    } xkb;

    // key repeat data
    struct repeat {
        int fd;           ///< Timer file descriptor
        xkb_keysym_t key; ///< Key to repeat
        uint32_t rate;    ///< Rate of repeating keys in characters per second
        uint32_t delay;   ///< Delay in milliseconds
    } repeat;

    struct surface {
        struct wl_buffer* buffer;
        void* data;
    } surface;

    // window size and its scale factor
    struct wnd {
        size_t width;
        size_t height;
        int32_t scale;
    } wnd;

    // outputs and their scale factors
    struct outputs {
        struct wl_output* output;
        int32_t scale;
    } outputs[MAX_OUTPUTS];

    struct wnd_handlers handlers;

    enum state state;
};

static struct context ctx = { .wnd = { .scale = 1 }, .repeat = { .fd = -1 } };

/** Redraw window */
static void redraw(void)
{
    ctx.handlers.on_redraw(ctx.wnd.width, ctx.wnd.height, ctx.surface.data);
    wl_surface_attach(ctx.wl.surface, ctx.surface.buffer, 0, 0);
    wl_surface_damage(ctx.wl.surface, 0, 0, ctx.wnd.width, ctx.wnd.height);
    wl_surface_set_buffer_scale(ctx.wl.surface, ctx.wnd.scale);
    wl_surface_commit(ctx.wl.surface);
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
 * (Re)create buffer.
 * @return true if operation completed successfully
 */
static bool create_buffer(void)
{
    const size_t stride = ctx.wnd.width * sizeof(argb_t);
    const size_t buf_sz = stride * ctx.wnd.height;
    struct wl_shm_pool* pool;
    int fd;
    bool status;

    // free previous allocated buffer
    if (ctx.surface.buffer) {
        wl_buffer_destroy(ctx.surface.buffer);
        ctx.surface.buffer = NULL;
    }

    // create new buffer
    fd = create_shmem(buf_sz, &ctx.surface.data);
    status = (fd != -1);
    if (status) {
        pool = wl_shm_create_pool(ctx.wl.shm, fd, buf_sz);
        close(fd);
        ctx.surface.buffer =
            wl_shm_pool_create_buffer(pool, 0, ctx.wnd.width, ctx.wnd.height,
                                      stride, WL_SHM_FORMAT_ARGB8888);
        wl_shm_pool_destroy(pool);
    }

    return status;
}

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
    xkb_state_update_mask(ctx.xkb.state, mods_depressed, mods_latched,
                          mods_locked, 0, 0, group);
}

static void on_keyboard_repeat_info(void* data, struct wl_keyboard* wl_keyboard,
                                    int32_t rate, int32_t delay)
{
    // save keyboard repeat preferences
    ctx.repeat.rate = rate;
    ctx.repeat.delay = delay;
}

static void on_keyboard_keymap(void* data, struct wl_keyboard* wl_keyboard,
                               uint32_t format, int32_t fd, uint32_t size)
{
    xkb_state_unref(ctx.xkb.state);
    xkb_keymap_unref(ctx.xkb.keymap);

    char* keymap = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);

    ctx.xkb.keymap = xkb_keymap_new_from_string(ctx.xkb.context, keymap,
                                                XKB_KEYMAP_FORMAT_TEXT_V1,
                                                XKB_KEYMAP_COMPILE_NO_FLAGS);
    ctx.xkb.state = xkb_state_new(ctx.xkb.keymap);

    munmap(keymap, size);
    close(fd);
}

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

static void on_keyboard_key(void* data, struct wl_keyboard* wl_keyboard,
                            uint32_t serial, uint32_t time, uint32_t key,
                            uint32_t state)
{
    struct itimerspec ts = { 0 };

    if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        // stop key repeat timer
        timerfd_settime(ctx.repeat.fd, 0, &ts, NULL);
    } else if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        xkb_keysym_t keysym;
        key += 8;
        keysym = xkb_state_key_get_one_sym(ctx.xkb.state, key);
        if (keysym != XKB_KEY_NoSymbol) {
            // handle key in viewer
            if (ctx.handlers.on_keyboard(keysym)) {
                redraw();
            }
            // handle key repeat
            if (ctx.repeat.rate &&
                xkb_keymap_key_repeats(ctx.xkb.keymap, key)) {
                // start key repeat timer
                ctx.repeat.key = keysym;
                set_timespec(&ts.it_value, ctx.repeat.delay);
                set_timespec(&ts.it_interval, 1000 / ctx.repeat.rate);
                timerfd_settime(ctx.repeat.fd, 0, &ts, NULL);
            }
        }
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

/*******************************************************************************
 * Seat handlers
 ******************************************************************************/
static void on_seat_name(void* data, struct wl_seat* seat, const char* name) { }

static void on_seat_capabilities(void* data, struct wl_seat* seat, uint32_t cap)
{
    if (cap & WL_SEAT_CAPABILITY_KEYBOARD) {
        ctx.wl.keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(ctx.wl.keyboard, &keyboard_listener, NULL);
    } else if (ctx.wl.keyboard) {
        wl_keyboard_destroy(ctx.wl.keyboard);
        ctx.wl.keyboard = NULL;
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
    xdg_surface_ack_configure(surface, serial);

    if (!ctx.surface.buffer && !create_buffer()) {
        ctx.state = state_error;
        return;
    }

    redraw();
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
    const int32_t cur_width = (int32_t)(ctx.wnd.width / ctx.wnd.scale);
    const int32_t cur_height = (int32_t)(ctx.wnd.height / ctx.wnd.scale);
    if (width && height && (width != cur_width || height != cur_height)) {
        ctx.wnd.width = width * ctx.wnd.scale;
        ctx.wnd.height = height * ctx.wnd.scale;
        if (create_buffer()) {
            ctx.handlers.on_resize(ctx.wnd.width, ctx.wnd.height);
        } else {
            ctx.state = state_error;
        }
    }
}

static void handle_xdg_toplevel_close(void* data, struct xdg_toplevel* top)
{
    ctx.state = state_exit;
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
    // save output scale factor
    for (size_t i = 0; i < sizeof(ctx.outputs) / sizeof(ctx.outputs[0]); ++i) {
        if (!ctx.outputs[i].output || ctx.outputs[i].output == output) {
            ctx.outputs[i].output = output;
            ctx.outputs[i].scale = factor;
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
    int32_t scale = 1;
    for (size_t i = 0; i < sizeof(ctx.outputs) / sizeof(ctx.outputs[0]); ++i) {
        if (ctx.outputs[i].output == output) {
            scale = ctx.outputs[i].scale;
            break;
        }
    }
    if (scale != ctx.wnd.scale) {
        ctx.wnd.width = (ctx.wnd.width / ctx.wnd.scale) * scale;
        ctx.wnd.height = (ctx.wnd.height / ctx.wnd.scale) * scale;
        ctx.wnd.scale = scale;
        if (create_buffer()) {
            ctx.handlers.on_resize(ctx.wnd.width, ctx.wnd.height);
            redraw();
        } else {
            ctx.state = state_error;
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
    if (strcmp(interface, wl_shm_interface.name) == 0) {
        ctx.wl.shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, wl_compositor_interface.name) == 0) {
        ctx.wl.compositor =
            wl_registry_bind(registry, name, &wl_compositor_interface, 3);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        struct wl_output* output =
            wl_registry_bind(registry, name, &wl_output_interface, 3);
        wl_output_add_listener(output, &wl_output_listener, NULL);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        ctx.xdg.base =
            wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(ctx.xdg.base, &xdg_base_listener, NULL);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        ctx.wl.seat = wl_registry_bind(registry, name, &wl_seat_interface, 4);
        wl_seat_add_listener(ctx.wl.seat, &seat_listener, NULL);
    }
}

void on_registry_remove(void* data, struct wl_registry* registry, uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
    .global = on_registry_global, .global_remove = on_registry_remove
};

bool create_window(const struct wnd_handlers* handlers, size_t width,
                   size_t height, const char* app_id)
{
    ctx.wnd.width = width ? width : 800;
    ctx.wnd.height = height ? height : 600;
    ctx.handlers = *handlers;

    ctx.wl.display = wl_display_connect(NULL);
    if (!ctx.wl.display) {
        fprintf(stderr, "Failed to open display\n");
        return false;
    }
    ctx.xkb.context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    ctx.wl.registry = wl_display_get_registry(ctx.wl.display);
    if (!ctx.wl.registry) {
        fprintf(stderr, "Failed to open registry\n");
        destroy_window();
        return false;
    }
    wl_registry_add_listener(ctx.wl.registry, &registry_listener, NULL);
    wl_display_roundtrip(ctx.wl.display);

    ctx.wl.surface = wl_compositor_create_surface(ctx.wl.compositor);
    if (!ctx.wl.surface) {
        fprintf(stderr, "Failed to create surface\n");
        destroy_window();
        return false;
    }
    wl_surface_add_listener(ctx.wl.surface, &wl_surface_listener, NULL);

    ctx.xdg.surface = xdg_wm_base_get_xdg_surface(ctx.xdg.base, ctx.wl.surface);
    if (!ctx.xdg.surface) {
        fprintf(stderr, "Failed to create xdg surface\n");
        destroy_window();
        return false;
    }
    xdg_surface_add_listener(ctx.xdg.surface, &xdg_surface_listener, NULL);
    ctx.xdg.toplevel = xdg_surface_get_toplevel(ctx.xdg.surface);
    xdg_toplevel_add_listener(ctx.xdg.toplevel, &xdg_toplevel_listener, NULL);
    xdg_toplevel_set_app_id(ctx.xdg.toplevel, app_id);

    wl_surface_commit(ctx.wl.surface);

    ctx.repeat.fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);

    return true;
}

void show_window(void)
{
    // file descriptors to poll
    struct pollfd fds[] = {
        /* 0 */ { .fd = wl_display_get_fd(ctx.wl.display), .events = POLLIN },
        /* 1 */ { .fd = ctx.repeat.fd, .events = POLLIN },
    };

    while (ctx.state == state_ok) {
        // prepare to read wayland events
        while (wl_display_prepare_read(ctx.wl.display) != 0) {
            wl_display_dispatch_pending(ctx.wl.display);
        }
        wl_display_flush(ctx.wl.display);

        // poll events
        if (poll(fds, sizeof(fds) / sizeof(fds[0]), -1) <= 0) {
            wl_display_cancel_read(ctx.wl.display);
            continue;
        }

        // read and handle wayland events
        if (fds[0].revents & POLLIN) {
            wl_display_read_events(ctx.wl.display);
            wl_display_dispatch_pending(ctx.wl.display);
        } else {
            wl_display_cancel_read(ctx.wl.display);
        }

        // read and handle key repeat events from timer
        if (fds[1].revents & POLLIN) {
            uint64_t repeats;
            if (read(ctx.repeat.fd, &repeats, sizeof(repeats)) ==
                sizeof(repeats)) {
                bool handled = false;
                while (repeats--) {
                    handled |= ctx.handlers.on_keyboard(ctx.repeat.key);
                }
                if (handled) {
                    redraw();
                }
            }
        }
    }
}

void destroy_window(void)
{
    if (ctx.repeat.fd != -1) {
        close(ctx.repeat.fd);
    }
    if (ctx.xkb.state) {
        xkb_state_unref(ctx.xkb.state);
    }
    if (ctx.xkb.keymap) {
        xkb_keymap_unref(ctx.xkb.keymap);
    }
    if (ctx.xkb.context) {
        xkb_context_unref(ctx.xkb.context);
    }
    if (ctx.surface.buffer) {
        wl_buffer_destroy(ctx.surface.buffer);
    }
    if (ctx.wl.seat) {
        wl_seat_destroy(ctx.wl.seat);
    }
    if (ctx.wl.keyboard) {
        wl_keyboard_destroy(ctx.wl.keyboard);
    }
    if (ctx.wl.shm) {
        wl_shm_destroy(ctx.wl.shm);
    }
    if (ctx.xdg.base) {
        xdg_surface_destroy(ctx.xdg.surface);
    }
    if (ctx.xdg.base) {
        xdg_wm_base_destroy(ctx.xdg.base);
    }
    if (ctx.wl.compositor) {
        wl_compositor_destroy(ctx.wl.compositor);
    }
    if (ctx.wl.registry) {
        wl_registry_destroy(ctx.wl.registry);
    }
    wl_display_disconnect(ctx.wl.display);
}

void close_window(void)
{
    ctx.state = state_exit;
}

void set_window_title(const char* file)
{
    const char* fmt = APP_NAME ": %s";
    char* title;
    int len;

    if (!ctx.xdg.toplevel) {
        return;
    }

    len = snprintf(NULL, 0, fmt, file);
    if (len > 0) {
        ++len; // last null
        title = malloc(len);
        if (title) {
            sprintf(title, fmt, file);
            xdg_toplevel_set_title(ctx.xdg.toplevel, title);
            free(title);
        }
    }
}

void enable_fullscreen(bool enable)
{
    if (enable) {
        xdg_toplevel_set_fullscreen(ctx.xdg.toplevel, NULL);
    } else {
        xdg_toplevel_unset_fullscreen(ctx.xdg.toplevel);
    }
}
