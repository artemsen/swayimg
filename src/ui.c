// SPDX-License-Identifier: MIT
// User interface: Window management, keyboard input, etc.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "ui.h"

#include "application.h"
#include "buildcfg.h"
#include "config.h"
#include "xdg-shell-protocol.h"
#include "wp-content-type-v1-protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <unistd.h>

// Max number of output displays
#define MAX_OUTPUTS 4

// Window size
#define WINDOW_MIN            10
#define WINDOW_MAX            100000
#define WINDOW_DEFAULT_WIDTH  800
#define WINDOW_DEFAULT_HEIGHT 600

// Mouse buttons, from <linux/input-event-codes.h>
#ifndef BTN_LEFT
#define BTN_LEFT   0x110
#define BTN_RIGHT  0x111
#define BTN_MIDDLE 0x112
#define BTN_SIDE   0x113
#define BTN_EXTRA  0x114
#endif

// Uncomment the following line to enable printing draw time
// #define TRACE_DRAW_TIME

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
        struct wp_content_type_manager_v1* content_type_manager;
        struct wl_output* output;
    } wl;

    // outputs and their scale factors
    struct outputs {
        struct wl_output* output;
        int32_t scale;
    } outputs[MAX_OUTPUTS];

    // window buffers
    struct wnd {
        struct wl_buffer* buffer0;
        struct wl_buffer* buffer1;
        struct wl_buffer* current;
        struct pixmap pm;
        size_t width;
        size_t height;
        int32_t scale;
#ifdef TRACE_DRAW_TIME
        struct timespec draw_time;
#endif
    } wnd;

    // cross-desktop
    struct xdg {
        bool initialized;
        struct xdg_wm_base* base;
        struct xdg_surface* surface;
        struct xdg_toplevel* toplevel;
    } xdg;

    // keyboard
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

    // mouse drag
    struct mouse {
        bool active;
        int x;
        int y;
    } mouse;

    // fullscreen mode
    bool fullscreen;

    // flag to cancel event queue
    bool event_handled;
};

/** Global UI context instance. */
static struct ui ctx = {
    .wnd.scale = 1,
    .repeat.fd = -1,
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
 * Create window buffer.
 * @return wayland buffer on NULL on errors
 */
static struct wl_buffer* create_buffer(void)
{
    const size_t stride = ctx.wnd.width * sizeof(argb_t);
    const size_t buf_sz = stride * ctx.wnd.height;
    struct wl_buffer* buffer = NULL;
    struct wl_shm_pool* pool = NULL;
    int fd = -1;
    char path[64];
    struct timespec ts;
    void* data;

    // generate unique file name
    clock_gettime(CLOCK_MONOTONIC, &ts);
    snprintf(path, sizeof(path), "/" APP_NAME "_%" PRIx64,
             ((uint64_t)ts.tv_sec << 32) | ts.tv_nsec);

    // open shared mem
    fd = shm_open(path, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd == -1) {
        fprintf(stderr, "Unable to create shared file: [%i] %s\n", errno,
                strerror(errno));
        return NULL;
    }
    shm_unlink(path);

    // set shared memory size
    if (ftruncate(fd, buf_sz) == -1) {
        fprintf(stderr, "Unable to truncate shared file: [%i] %s\n", errno,
                strerror(errno));
        close(fd);
        return NULL;
    }

    // get data pointer of the shared mem
    data = mmap(NULL, buf_sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        fprintf(stderr, "Unable to map shared file: [%i] %s\n", errno,
                strerror(errno));
        close(fd);
        return NULL;
    }

    // create wayland buffer
    pool = wl_shm_create_pool(ctx.wl.shm, fd, buf_sz);
    buffer = wl_shm_pool_create_buffer(pool, 0, ctx.wnd.width, ctx.wnd.height,
                                       stride, WL_SHM_FORMAT_ARGB8888);
    wl_buffer_set_user_data(buffer, data);

    wl_shm_pool_destroy(pool);
    close(fd);

    return buffer;
}

/**
 * Free window buffer.
 * @param buffer wayland buffer to free
 */
static void free_buffer(struct wl_buffer* buffer)
{
    if (buffer) {
        const size_t stride = ctx.wnd.pm.width * sizeof(argb_t);
        const size_t size = stride * ctx.wnd.pm.height;
        void* data = wl_buffer_get_user_data(buffer);
        wl_buffer_destroy(buffer);
        munmap(data, size);
    }
}

/**
 * Recreate window buffers.
 * @return true if operation completed successfully
 */
static bool recreate_buffers(void)
{
    ctx.wnd.current = NULL;

    // recreate buffers
    free_buffer(ctx.wnd.buffer0);
    ctx.wnd.buffer0 = create_buffer();
    free_buffer(ctx.wnd.buffer1);
    ctx.wnd.buffer1 = create_buffer();
    if (!ctx.wnd.buffer0 || !ctx.wnd.buffer1) {
        return false;
    }

    ctx.wnd.pm.width = ctx.wnd.width;
    ctx.wnd.pm.height = ctx.wnd.height;

    ctx.wnd.current = ctx.wnd.buffer0;

    app_on_resize();

    return true;
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
    // reset keyboard repeat timer
    struct itimerspec ts = { 0 };
    timerfd_settime(ctx.repeat.fd, 0, &ts, NULL);
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
    char* keymap;

    xkb_state_unref(ctx.xkb.state);
    xkb_keymap_unref(ctx.xkb.keymap);

    keymap = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    ctx.xkb.keymap = xkb_keymap_new_from_string(ctx.xkb.context, keymap,
                                                XKB_KEYMAP_FORMAT_TEXT_V1,
                                                XKB_KEYMAP_COMPILE_NO_FLAGS);
    ctx.xkb.state = xkb_state_new(ctx.xkb.keymap);

    munmap(keymap, size);
    close(fd);
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
            app_on_keyboard(keysym, keybind_mods(ctx.xkb.state));
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
    const int x = wl_fixed_to_int(surface_x);
    const int y = wl_fixed_to_int(surface_y);

    if (ctx.mouse.active) {
        const int dx = x - ctx.mouse.x;
        const int dy = y - ctx.mouse.y;
        if (dx || dy) {
            app_on_drag(dx, dy);
        }
    }

    ctx.mouse.x = x;
    ctx.mouse.y = y;
}

static void on_pointer_button(void* data, struct wl_pointer* wl_pointer,
                              uint32_t serial, uint32_t time, uint32_t button,
                              uint32_t state)
{
    const bool pressed = (state == WL_POINTER_BUTTON_STATE_PRESSED);

    if (button == BTN_LEFT) {
        ctx.mouse.active = pressed;
    }

    if (pressed) {
        xkb_keysym_t key;
        switch (button) {
            // TODO: Configurable drag
            // case BTN_LEFT:
            //     key = VKEY_MOUSE_LEFT;
            //     break;
            case BTN_RIGHT:
                key = VKEY_MOUSE_RIGHT;
                break;
            case BTN_MIDDLE:
                key = VKEY_MOUSE_MIDDLE;
                break;
            case BTN_SIDE:
                key = VKEY_MOUSE_SIDE;
                break;
            case BTN_EXTRA:
                key = VKEY_MOUSE_EXTRA;
                break;
            default:
                key = XKB_KEY_NoSymbol;
                break;
        }
        if (key != XKB_KEY_NoSymbol) {
            app_on_keyboard(key, keybind_mods(ctx.xkb.state));
        }
    }
}

static void on_pointer_axis(void* data, struct wl_pointer* wl_pointer,
                            uint32_t time, uint32_t axis, wl_fixed_t value)
{
    xkb_keysym_t key;

    if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
        key = value > 0 ? VKEY_SCROLL_RIGHT : VKEY_SCROLL_LEFT;
    } else {
        key = value > 0 ? VKEY_SCROLL_DOWN : VKEY_SCROLL_UP;
    }

    app_on_keyboard(key, keybind_mods(ctx.xkb.state));
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
    if (cap & WL_SEAT_CAPABILITY_KEYBOARD) {
        ctx.wl.keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(ctx.wl.keyboard, &keyboard_listener, NULL);
    } else if (ctx.wl.keyboard) {
        wl_keyboard_destroy(ctx.wl.keyboard);
        ctx.wl.keyboard = NULL;
    }

    if (cap & WL_SEAT_CAPABILITY_POINTER) {
        ctx.wl.pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(ctx.wl.pointer, &pointer_listener, NULL);
    } else if (ctx.wl.pointer) {
        wl_pointer_destroy(ctx.wl.pointer);
        ctx.wl.pointer = NULL;
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

    if (ctx.xdg.initialized) {
        app_redraw();
    } else {
        wl_surface_attach(ctx.wl.surface, ctx.wnd.current, 0, 0);
        wl_surface_commit(ctx.wl.surface);
    }
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
    bool reset_buffers = (ctx.wnd.current == NULL);

    if (width && height) {
        const size_t new_width = width * ctx.wnd.scale;
        const size_t new_height = height * ctx.wnd.scale;
        if (width != (int32_t)ctx.wnd.width ||
            height != (int32_t)ctx.wnd.height) {
            ctx.wnd.width = new_width;
            ctx.wnd.height = new_height;
            reset_buffers = true;
        }
        ctx.xdg.initialized = true;
    }

    if (reset_buffers && !recreate_buffers()) {
        app_exit(1);
    }
}

static void handle_xdg_toplevel_close(void* data, struct xdg_toplevel* top)
{
    app_exit(0);
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
    for (size_t i = 0; i < MAX_OUTPUTS; ++i) {
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

    // find scale factor for current output
    for (size_t i = 0; i < MAX_OUTPUTS; ++i) {
        if (ctx.outputs[i].output == output) {
            scale = ctx.outputs[i].scale;
            break;
        }
    }

    // recreate buffer if scale has changed
    if (scale != ctx.wnd.scale) {
        ctx.wnd.width = (ctx.wnd.width / ctx.wnd.scale) * scale;
        ctx.wnd.height = (ctx.wnd.height / ctx.wnd.scale) * scale;
        ctx.wnd.scale = scale;
        if (recreate_buffers()) {
            app_redraw();
        } else {
            app_exit(1);
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
        if (!ctx.wl.output) {
            ctx.wl.output =
                wl_registry_bind(registry, name, &wl_output_interface, 3);
            wl_output_add_listener(ctx.wl.output, &wl_output_listener, data);
        }
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        ctx.xdg.base =
            wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(ctx.xdg.base, &xdg_base_listener, data);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        ctx.wl.seat = wl_registry_bind(registry, name, &wl_seat_interface, 4);
        wl_seat_add_listener(ctx.wl.seat, &seat_listener, data);
    } else if (strcmp(interface, wp_content_type_manager_v1_interface.name) == 0) {
        ctx.wl.content_type_manager = wl_registry_bind(registry, name, &wp_content_type_manager_v1_interface, 1);
    }
}

static void on_registry_remove(void* data, struct wl_registry* registry,
                               uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
    .global = on_registry_global, .global_remove = on_registry_remove
};

#pragma GCC diagnostic pop // "-Wunused-parameter"

// Key repeat handler
static void on_key_repeat(__attribute__((unused)) void* data)
{
    uint64_t repeats;
    const ssize_t sz = sizeof(repeats);
    if (read(ctx.repeat.fd, &repeats, sz) == sz) {
        app_on_keyboard(ctx.repeat.key, keybind_mods(ctx.xkb.state));
    }
}

// Wayland event handler
static void on_wayland_event(__attribute__((unused)) void* data)
{
    wl_display_read_events(ctx.wl.display);
    wl_display_dispatch_pending(ctx.wl.display);
    ctx.event_handled = true;
}

bool ui_init(const char* app_id, size_t width, size_t height)
{
    ctx.wnd.width = width;
    ctx.wnd.height = height;
    if (ctx.wnd.width < WINDOW_MIN || ctx.wnd.height < WINDOW_MIN ||
        ctx.wnd.width > WINDOW_MAX || ctx.wnd.height > WINDOW_MAX) {
        ctx.wnd.width = WINDOW_DEFAULT_WIDTH;
        ctx.wnd.height = WINDOW_DEFAULT_HEIGHT;
    }

    ctx.wl.display = wl_display_connect(NULL);
    if (!ctx.wl.display) {
        fprintf(stderr, "Failed to open display\n");
        return false;
    }
    ctx.xkb.context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    ctx.wl.registry = wl_display_get_registry(ctx.wl.display);
    if (!ctx.wl.registry) {
        fprintf(stderr, "Failed to open registry\n");
        ui_destroy();
        return false;
    }
    wl_registry_add_listener(ctx.wl.registry, &registry_listener, NULL);
    wl_display_roundtrip(ctx.wl.display);

    ctx.wl.surface = wl_compositor_create_surface(ctx.wl.compositor);
    if (!ctx.wl.surface) {
        fprintf(stderr, "Failed to create surface\n");
        ui_destroy();
        return false;
    }
    wl_surface_add_listener(ctx.wl.surface, &wl_surface_listener, NULL);

    ctx.xdg.surface = xdg_wm_base_get_xdg_surface(ctx.xdg.base, ctx.wl.surface);
    if (!ctx.xdg.surface) {
        fprintf(stderr, "Failed to create xdg surface\n");
        ui_destroy();
        return false;
    }
    xdg_surface_add_listener(ctx.xdg.surface, &xdg_surface_listener, NULL);
    ctx.xdg.toplevel = xdg_surface_get_toplevel(ctx.xdg.surface);
    xdg_toplevel_add_listener(ctx.xdg.toplevel, &xdg_toplevel_listener, NULL);
    xdg_toplevel_set_app_id(ctx.xdg.toplevel, app_id);
    if (ctx.fullscreen) {
        xdg_toplevel_set_fullscreen(ctx.xdg.toplevel, NULL);
    }

    if (ctx.wl.content_type_manager) {
        struct wp_content_type_v1* content_type;
        content_type = wp_content_type_manager_v1_get_surface_content_type(ctx.wl.content_type_manager, ctx.wl.surface);
        wp_content_type_v1_set_content_type(content_type, WP_CONTENT_TYPE_V1_TYPE_PHOTO);
    }

    wl_surface_commit(ctx.wl.surface);

    app_watch(wl_display_get_fd(ctx.wl.display), on_wayland_event, NULL);

    ctx.repeat.fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    app_watch(ctx.repeat.fd, on_key_repeat, NULL);

    return true;
}

void ui_destroy(void)
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
    free_buffer(ctx.wnd.buffer0);
    free_buffer(ctx.wnd.buffer1);
    if (ctx.wl.seat) {
        wl_seat_destroy(ctx.wl.seat);
    }
    if (ctx.wl.keyboard) {
        wl_keyboard_destroy(ctx.wl.keyboard);
    }
    if (ctx.wl.pointer) {
        wl_pointer_destroy(ctx.wl.pointer);
    }
    if (ctx.wl.shm) {
        wl_shm_destroy(ctx.wl.shm);
    }
    if (ctx.xdg.toplevel) {
        xdg_toplevel_destroy(ctx.xdg.toplevel);
    }
    if (ctx.xdg.base) {
        xdg_surface_destroy(ctx.xdg.surface);
    }
    if (ctx.xdg.base) {
        xdg_wm_base_destroy(ctx.xdg.base);
    }
    if (ctx.wl.output) {
        wl_output_destroy(ctx.wl.output);
    }
    if (ctx.wl.surface) {
        wl_surface_destroy(ctx.wl.surface);
    }
    if (ctx.wl.compositor) {
        wl_compositor_destroy(ctx.wl.compositor);
    }
    if (ctx.wl.content_type_manager) {
        wp_content_type_manager_v1_destroy(ctx.wl.content_type_manager);
    }
    if (ctx.wl.registry) {
        wl_registry_destroy(ctx.wl.registry);
    }
    if (ctx.wl.display) {
        wl_display_disconnect(ctx.wl.display);
    }
}

void ui_event_prepare(void)
{
    ctx.event_handled = false;

    while (wl_display_prepare_read(ctx.wl.display) != 0) {
        wl_display_dispatch_pending(ctx.wl.display);
    }

    wl_display_flush(ctx.wl.display);
}

void ui_event_done(void)
{
    if (!ctx.event_handled) {
        wl_display_cancel_read(ctx.wl.display);
    }
}

struct pixmap* ui_draw_begin(void)
{
    if (!ctx.wnd.current) {
        return NULL; // not yet initialized
    }

    // switch buffers
    if (ctx.wnd.current == ctx.wnd.buffer0) {
        ctx.wnd.current = ctx.wnd.buffer1;
    } else {
        ctx.wnd.current = ctx.wnd.buffer0;
    }

    ctx.wnd.pm.data = wl_buffer_get_user_data(ctx.wnd.current);

#ifdef TRACE_DRAW_TIME
    clock_gettime(CLOCK_MONOTONIC, &ctx.wnd.draw_time);
#endif

    return &ctx.wnd.pm;
}

void ui_draw_commit(void)
{
#ifdef TRACE_DRAW_TIME
    struct timespec curr;
    clock_gettime(CLOCK_MONOTONIC, &curr);
    const double ns = (curr.tv_sec * 1000000000 + curr.tv_nsec) -
        (ctx.wnd.draw_time.tv_sec * 1000000000 + ctx.wnd.draw_time.tv_nsec);
    printf("Rendered in %.6f sec\n", ns / 1000000000);
#endif

    wl_surface_attach(ctx.wl.surface, ctx.wnd.current, 0, 0);
    wl_surface_damage(ctx.wl.surface, 0, 0, ctx.wnd.width, ctx.wnd.height);
    wl_surface_set_buffer_scale(ctx.wl.surface, ctx.wnd.scale);
    wl_surface_commit(ctx.wl.surface);
}

void ui_set_title(const char* name)
{
    char* title = NULL;

    str_append(APP_NAME ": ", 0, &title);
    str_append(name, 0, &title);

    if (title) {
        xdg_toplevel_set_title(ctx.xdg.toplevel, title);
        free(title);
    }
}

size_t ui_get_width(void)
{
    return ctx.wnd.width;
}

size_t ui_get_height(void)
{
    return ctx.wnd.height;
}

size_t ui_get_scale(void)
{
    return ctx.wnd.scale;
}

void ui_toggle_fullscreen(void)
{
    ctx.fullscreen = !ctx.fullscreen;

    if (ctx.xdg.toplevel) {
        if (ctx.fullscreen) {
            xdg_toplevel_set_fullscreen(ctx.xdg.toplevel, NULL);
        } else {
            xdg_toplevel_unset_fullscreen(ctx.xdg.toplevel);
        }
    }
}
