// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

// ftruncate() support
#define _POSIX_C_SOURCE 200112

#include "window.h"
#include "xdg-shell-protocol.h"
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <linux/input.h>

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

    struct surface {
        cairo_surface_t* cairo;
        struct wl_buffer* buffer;
    } surface;

    struct size {
        size_t width;
        size_t height;
    } size;

    struct handlers {
        on_redraw redraw;
        on_keyboard keyboard;
    } handlers;

    enum state state;
};

static struct context ctx;

/** Redraw window */
static void redraw(void)
{
    ctx.handlers.redraw(ctx.surface.cairo);
    wl_surface_attach(ctx.wl.surface, ctx.surface.buffer, 0, 0);
    wl_surface_damage(ctx.wl.surface, 0, 0, ctx.size.width, ctx.size.height);
    wl_surface_commit(ctx.wl.surface);
}

/**
 * Create shared memory file.
 * @param[in] sz size of data in bytes
 * @param[out] data pointer to mapped data
 * @return shared file descriptor, -1 on errors
 */
static int create_shmem(size_t sz, void** data)
{
    char path[64];
    struct timeval tv;
    gettimeofday(&tv, NULL);
    snprintf(path, sizeof(path), "/" APP_NAME "_%lx", tv.tv_sec << 32 | tv.tv_usec);

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

/*******************************************************************************
 * Keyboard handlers
 ******************************************************************************/
static void on_keyboard_enter(void* data, struct wl_keyboard* wl_keyboard,
                              uint32_t serial, struct wl_surface* surface,
                              struct wl_array* keys)
{}

static void on_keyboard_leave(void* data, struct wl_keyboard* wl_keyboard,
                              uint32_t serial, struct wl_surface* surface)
{}

static void on_keyboard_modifiers(void* data, struct wl_keyboard* wl_keyboard,
                                  uint32_t serial, uint32_t mods_depressed,
                                  uint32_t mods_latched, uint32_t mods_locked,
                                  uint32_t group)
{}

static void on_keyboard_repeat_info(void* data, struct wl_keyboard* wl_keyboard,
                                    int32_t rate, int32_t delay)
{}

static void on_keyboard_keymap(void* data, struct wl_keyboard* wl_keyboard,
                               uint32_t format, int32_t fd, uint32_t size)
{
    close(fd);
}

static void on_keyboard_key(void* data, struct wl_keyboard* wl_keyboard,
                            uint32_t serial, uint32_t time, uint32_t key,
                            uint32_t state)
{
    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        if (ctx.handlers.keyboard(key)) {
            redraw();
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
static void on_seat_name(void* data, struct wl_seat* seat, const char* name)
{}

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

static const struct wl_seat_listener seat_listener = {
    .capabilities = on_seat_capabilities,
    .name = on_seat_name
};


/*******************************************************************************
 * XDG handlers
 ******************************************************************************/
static void on_xdg_surface_configure(void* data, struct xdg_surface* surface,
                                     uint32_t serial)
{
    xdg_surface_ack_configure(surface, serial);

    if (!ctx.surface.buffer) {
        // create shared buffer
        const size_t stride = ctx.size.width * 4 /* argb */;
        const size_t buf_sz = stride * ctx.size.height;
        void* buf_data;
        const int fd = create_shmem(buf_sz, &buf_data);
        if (fd == -1) {
            ctx.state = state_error;
            return;
        }
        struct wl_shm_pool* pool = wl_shm_create_pool(ctx.wl.shm, fd, buf_sz);
        close(fd);
        ctx.surface.buffer = wl_shm_pool_create_buffer(pool, 0, ctx.size.width,
            ctx.size.height, stride, WL_SHM_FORMAT_XRGB8888);
        wl_shm_pool_destroy(pool);
        ctx.surface.cairo = cairo_image_surface_create_for_data(buf_data,
            CAIRO_FORMAT_ARGB32, ctx.size.width, ctx.size.height, stride);
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
                                          struct wl_array* state)
{}

static void handle_xdg_toplevel_close(void* data, struct xdg_toplevel* top)
{
    ctx.state = state_exit;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = handle_xdg_toplevel_configure,
    .close = handle_xdg_toplevel_close,
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
        ctx.wl.compositor = wl_registry_bind(registry, name,
                                             &wl_compositor_interface, 1);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        ctx.xdg.base = wl_registry_bind(registry, name,
                                        &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(ctx.xdg.base, &xdg_base_listener, NULL);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        ctx.wl.seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
        wl_seat_add_listener(ctx.wl.seat, &seat_listener, NULL);
    }
}

void on_registry_remove(void* data, struct wl_registry* registry, uint32_t name)
{}

static const struct wl_registry_listener registry_listener = {
    .global = on_registry_global,
    .global_remove = on_registry_remove
};

bool show_window(const struct window* wnd)
{
    ctx.size.width = wnd->width;
    ctx.size.height = wnd->height;
    ctx.handlers.redraw = wnd->redraw;
    ctx.handlers.keyboard = wnd->keyboard;

    ctx.wl.display = wl_display_connect(NULL);
    if (!ctx.wl.display) {
        fprintf(stderr, "Failed to open display\n");
        return false;
    }
    ctx.wl.registry = wl_display_get_registry(ctx.wl.display);
    if (!ctx.wl.registry) {
        fprintf(stderr, "Failed to open registry\n");
        goto done;
    }
    wl_registry_add_listener(ctx.wl.registry, &registry_listener, NULL);
    wl_display_roundtrip(ctx.wl.display);

    ctx.wl.surface = wl_compositor_create_surface(ctx.wl.compositor);
    if (!ctx.wl.surface) {
        fprintf(stderr, "Failed to create surface\n");
        goto done;
    }
    ctx.xdg.surface = xdg_wm_base_get_xdg_surface(ctx.xdg.base, ctx.wl.surface);
    if (!ctx.xdg.surface) {
        fprintf(stderr, "Failed to create xdg surface\n");
        goto done;
    }
    xdg_surface_add_listener(ctx.xdg.surface, &xdg_surface_listener, NULL);

    ctx.xdg.toplevel = xdg_surface_get_toplevel(ctx.xdg.surface);
    xdg_toplevel_add_listener(ctx.xdg.toplevel, &xdg_toplevel_listener, NULL);
    xdg_toplevel_set_title(ctx.xdg.toplevel, wnd->title);
    xdg_toplevel_set_app_id(ctx.xdg.toplevel, wnd->app_id);
    wl_surface_commit(ctx.wl.surface);

    // working loop
    while (ctx.state == state_ok) {
        wl_display_dispatch(ctx.wl.display);
    }

done:
    if (ctx.surface.cairo) {
        cairo_surface_destroy(ctx.surface.cairo);
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

    return ctx.state == state_exit;
}

void close_window(void)
{
    ctx.state = state_exit;
}
