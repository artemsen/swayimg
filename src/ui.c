// SPDX-License-Identifier: MIT
// User interface: Window management, keyboard input, etc.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "ui.h"

#include "buildcfg.h"
#include "config.h"
#include "str.h"
#include "viewer.h"
#include "xdg-shell-protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

// Max number of output displays
#define MAX_OUTPUTS 4

// Mouse button
#ifndef BTN_LEFT
#define BTN_LEFT 0x110 // from <linux/input-event-codes.h>
#endif

/** Loop state */
enum state {
    state_ok,
    state_exit,
    state_error,
};

/** Custom event */
struct custom_event {
    int fd;
    fd_event handler;
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

    // window buffers
    struct wnd {
        struct wl_buffer* buffer0;
        struct wl_buffer* buffer1;
        struct wl_buffer* current;
        ssize_t x;
        ssize_t y;
        size_t width;
        size_t height;
        int32_t scale;
    } wnd;

    // cross-desktop
    struct xdg {
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

    // app_id name (window class)
    char* app_id;

    // fullscreen mode
    bool fullscreen;

    // custom events
    struct custom_event* events;
    size_t num_events;

    // global state
    enum state state;
};

static struct ui ctx = {
    .wnd.scale = 1,
    .wnd.x = POS_FROM_PARENT,
    .wnd.y = POS_FROM_PARENT,
    .wnd.width = SIZE_FROM_PARENT,
    .wnd.height = SIZE_FROM_PARENT,
    .repeat.fd = -1,
    .state = state_ok,
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
    snprintf(path, sizeof(path), "/" APP_NAME "_%lx",
             (ts.tv_sec << 32) | ts.tv_nsec);

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
 * Recreate window buffers.
 * @return true if operation completed successfully
 */
static bool recreate_buffers(void)
{
    ctx.wnd.current = NULL;

    // first buffer
    if (ctx.wnd.buffer0) {
        wl_buffer_destroy(ctx.wnd.buffer0);
    }
    ctx.wnd.buffer0 = create_buffer();
    if (!ctx.wnd.buffer0) {
        return false;
    }
    // second buffer
    if (ctx.wnd.buffer1) {
        wl_buffer_destroy(ctx.wnd.buffer1);
    }
    ctx.wnd.buffer1 = create_buffer();
    if (!ctx.wnd.buffer1) {
        return false;
    }

    ctx.wnd.current = ctx.wnd.buffer0;
    viewer_on_resize(ctx.wnd.width, ctx.wnd.height, ctx.wnd.scale);
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
            viewer_on_keyboard(keysym);
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
        if (dx && dy) {
            viewer_on_drag(dx, dy);
        }
    }

    ctx.mouse.x = x;
    ctx.mouse.y = y;
}

static void on_pointer_button(void* data, struct wl_pointer* wl_pointer,
                              uint32_t serial, uint32_t time, uint32_t button,
                              uint32_t state)
{
    if (button == BTN_LEFT) {
        ctx.mouse.active = (state == WL_POINTER_BUTTON_STATE_PRESSED);
    }
}

static void on_pointer_axis(void* data, struct wl_pointer* wl_pointer,
                            uint32_t time, uint32_t axis, wl_fixed_t value)
{
    xkb_keysym_t key;
    if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
        key = value > 0 ? XKB_KEY_KP_Right : XKB_KEY_KP_Left;
    else
        key = value > 0 ? XKB_KEY_KP_Down : XKB_KEY_KP_Up;
    viewer_on_keyboard(key);
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

    if (!ctx.wnd.current && !recreate_buffers()) {
        ctx.state = state_error;
        return;
    }

    ui_redraw();
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
        if (!recreate_buffers()) {
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
            ui_redraw();
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

static bool create_window(void)
{
    if (ctx.wnd.width < 10 || ctx.wnd.height < 10) {
        // fixup window size
        ctx.wnd.width = 640;
        ctx.wnd.height = 480;
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
        ui_free();
        return false;
    }
    wl_registry_add_listener(ctx.wl.registry, &registry_listener, NULL);
    wl_display_roundtrip(ctx.wl.display);

    ctx.wl.surface = wl_compositor_create_surface(ctx.wl.compositor);
    if (!ctx.wl.surface) {
        fprintf(stderr, "Failed to create surface\n");
        ui_free();
        return false;
    }
    wl_surface_add_listener(ctx.wl.surface, &wl_surface_listener, NULL);

    ctx.xdg.surface = xdg_wm_base_get_xdg_surface(ctx.xdg.base, ctx.wl.surface);
    if (!ctx.xdg.surface) {
        fprintf(stderr, "Failed to create xdg surface\n");
        ui_free();
        return false;
    }
    xdg_surface_add_listener(ctx.xdg.surface, &xdg_surface_listener, NULL);
    ctx.xdg.toplevel = xdg_surface_get_toplevel(ctx.xdg.surface);
    xdg_toplevel_add_listener(ctx.xdg.toplevel, &xdg_toplevel_listener, NULL);
    xdg_toplevel_set_app_id(ctx.xdg.toplevel, ctx.app_id);
    if (ctx.fullscreen) {
        xdg_toplevel_set_fullscreen(ctx.xdg.toplevel, NULL);
    }

    wl_surface_commit(ctx.wl.surface);

    ctx.repeat.fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);

    return true;
}

/**
 * Custom section loader, see `config_loader` for details.
 */
static enum config_status load_config(const char* key, const char* value)
{
    enum config_status status = cfgst_invalid_value;

    if (strcmp(key, UI_CFG_APP_ID) == 0) {
        str_dup(value, &ctx.app_id);
        status = cfgst_ok;
    } else if (strcmp(key, UI_CFG_FULLSCREEN) == 0) {
        if (config_to_bool(value, &ctx.fullscreen)) {
            status = cfgst_ok;
        }
    } else if (strcmp(key, UI_CFG_SIZE) == 0) {
        ssize_t width, height;
        if (strcmp(value, "parent") == 0) {
            ctx.wnd.width = SIZE_FROM_PARENT;
            ctx.wnd.height = SIZE_FROM_PARENT;
            status = cfgst_ok;
        } else if (strcmp(value, "image") == 0) {
            ctx.wnd.width = SIZE_FROM_IMAGE;
            ctx.wnd.height = SIZE_FROM_IMAGE;
            status = cfgst_ok;
        } else {
            struct str_slice slices[2];
            if (str_split(value, ',', slices, 2) == 2 &&
                str_to_num(slices[0].value, slices[0].len, &width, 0) &&
                str_to_num(slices[1].value, slices[1].len, &height, 0) &&
                width > 0 && width < 100000 && height > 0 && height < 100000) {
                ctx.wnd.width = width;
                ctx.wnd.height = height;
                status = cfgst_ok;
            }
        }
    } else if (strcmp(key, UI_CFG_POSITION) == 0) {
        if (strcmp(value, "parent") == 0) {
            ctx.wnd.x = POS_FROM_PARENT;
            ctx.wnd.y = POS_FROM_PARENT;
            status = cfgst_ok;
        } else {
            struct str_slice slices[2];
            ssize_t x, y;
            if (str_split(value, ',', slices, 2) == 2 &&
                str_to_num(slices[0].value, slices[0].len, &x, 0) &&
                str_to_num(slices[1].value, slices[1].len, &y, 0)) {
                ctx.wnd.x = (ssize_t)x;
                ctx.wnd.y = (ssize_t)y;
                status = cfgst_ok;
            }
        }
    } else {
        status = cfgst_invalid_key;
    }

    return status;
}

void ui_init(void)
{
    struct timespec ts;
    char app_id[64];
    size_t len;

    // create unique application id
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        const uint64_t timestamp = (ts.tv_sec << 32) | ts.tv_nsec;
        snprintf(app_id, sizeof(app_id), APP_NAME "_%lx", timestamp);
    } else {
        strncpy(app_id, APP_NAME, sizeof(app_id));
    }
    len = strlen(app_id) + 1;
    ctx.app_id = malloc(len);
    if (ctx.app_id) {
        memcpy(ctx.app_id, app_id, len);
    }

    // register configuration loader
    config_add_loader(GENERAL_CONFIG_SECTION, load_config);
}

void ui_free(void)
{
    free(ctx.app_id);
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
    if (ctx.wnd.buffer0) {
        wl_buffer_destroy(ctx.wnd.buffer0);
    }
    if (ctx.wnd.buffer1) {
        wl_buffer_destroy(ctx.wnd.buffer1);
    }
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
    if (ctx.wl.registry) {
        wl_registry_destroy(ctx.wl.registry);
    }
    if (ctx.wl.display) {
        wl_display_disconnect(ctx.wl.display);
    }
}

bool ui_run(void)
{
    const size_t num_fds = ctx.num_events + 2; // wayland + key repeat
    const size_t idx_wayland = num_fds - 1;
    const size_t idx_krepeat = num_fds - 2;
    struct pollfd* fds;

    if (!create_window()) {
        return false;
    }

    // file descriptors to poll
    fds = calloc(1, num_fds * sizeof(struct pollfd));
    if (!fds) {
        fprintf(stderr, "Not enough memory\n");
        return false;
    }
    for (size_t i = 0; i < ctx.num_events; ++i) {
        fds[i].fd = ctx.events[i].fd;
        fds[i].events = POLLIN;
    }
    fds[idx_wayland].fd = wl_display_get_fd(ctx.wl.display);
    fds[idx_wayland].events = POLLIN;
    fds[idx_krepeat].fd = ctx.repeat.fd;
    fds[idx_krepeat].events = POLLIN;

    // main event loop
    while (ctx.state == state_ok) {
        // prepare to read wayland events
        while (wl_display_prepare_read(ctx.wl.display) != 0) {
            wl_display_dispatch_pending(ctx.wl.display);
        }
        wl_display_flush(ctx.wl.display);

        // poll events
        if (poll(fds, num_fds, -1) <= 0) {
            wl_display_cancel_read(ctx.wl.display);
            continue;
        }

        // read and handle wayland events
        if (fds[idx_wayland].revents & POLLIN) {
            wl_display_read_events(ctx.wl.display);
            wl_display_dispatch_pending(ctx.wl.display);
        } else {
            wl_display_cancel_read(ctx.wl.display);
        }

        // read and handle key repeat events from timer
        if (fds[idx_krepeat].revents & POLLIN) {
            uint64_t repeats;
            const ssize_t sz = sizeof(repeats);
            if (read(ctx.repeat.fd, &repeats, sz) == sz) {
                while (repeats--) {
                    viewer_on_keyboard(ctx.repeat.key);
                }
            }
        }

        // read custom events
        for (size_t i = 0; i < ctx.num_events; ++i) {
            if (fds[i].revents & POLLIN) {
                ctx.events[i].handler();
            }
        }
    }

    free(fds);

    return ctx.state != state_error;
}

void ui_stop(void)
{
    ctx.state = state_exit;
}

void ui_redraw(void)
{
    argb_t* wnd_data;

    if (!ctx.wnd.current) {
        return; // not yet initialized
    }

    // switch buffers
    if (ctx.wnd.current == ctx.wnd.buffer0) {
        ctx.wnd.current = ctx.wnd.buffer1;
    } else {
        ctx.wnd.current = ctx.wnd.buffer0;
    }

    // draw to window buffer
    wnd_data = wl_buffer_get_user_data(ctx.wnd.current);
    viewer_on_redraw(wnd_data);

    // show window buffer
    wl_surface_attach(ctx.wl.surface, ctx.wnd.current, 0, 0);
    wl_surface_damage(ctx.wl.surface, 0, 0, ctx.wnd.width, ctx.wnd.height);
    wl_surface_set_buffer_scale(ctx.wl.surface, ctx.wnd.scale);
    wl_surface_commit(ctx.wl.surface);
}

const char* ui_get_appid(void)
{
    return ctx.app_id;
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

void ui_set_position(ssize_t x, ssize_t y)
{
    ctx.wnd.x = x;
    ctx.wnd.y = y;
}

ssize_t ui_get_x(void)
{
    return ctx.wnd.x;
}

ssize_t ui_get_y(void)
{
    return ctx.wnd.y;
}

void ui_set_size(size_t width, size_t height)
{
    ctx.wnd.width = width;
    ctx.wnd.height = height;
}

size_t ui_get_width(void)
{
    return ctx.wnd.width;
}

size_t ui_get_height(void)
{
    return ctx.wnd.height;
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

bool ui_get_fullscreen(void)
{
    return ctx.fullscreen;
}

void ui_add_event(int fd, fd_event handler)
{
    const size_t new_sz = (ctx.num_events + 1) * sizeof(struct custom_event);
    struct custom_event* events = realloc(ctx.events, new_sz);
    if (events) {
        ctx.events = events;
        ctx.events[ctx.num_events].fd = fd;
        ctx.events[ctx.num_events].handler = handler;
        ++ctx.num_events;
    }
}
