// SPDX-License-Identifier: MIT
// Wayland based user interface.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "ui_wayland.hpp"

#include "log.hpp"

#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <format>

/** Static Wayland handlers. */
class WaylandHandler {
public:
    /***************************************************************************
     * Keyboard handlers
     **************************************************************************/
    static void on_keyboard_enter(void*, struct wl_keyboard*, uint32_t,
                                  struct wl_surface*, struct wl_array*)
    {
    }

    static void on_keyboard_leave(void* data, struct wl_keyboard*, uint32_t,
                                  struct wl_surface*)
    {
        UiWayland* ui = reinterpret_cast<UiWayland*>(data);
        ui->xkb.stop_repeat();
    }

    static void on_keyboard_modifiers(void* data, struct wl_keyboard*, uint32_t,
                                      uint32_t mods_depressed,
                                      uint32_t mods_latched,
                                      uint32_t mods_locked, uint32_t group)
    {
        UiWayland* ui = reinterpret_cast<UiWayland*>(data);
        ui->xkb.update_modifiers(mods_depressed, mods_latched, mods_locked,
                                 group);
    }

    static void on_keyboard_repeat_info(void* data, struct wl_keyboard*,
                                        int32_t rate, int32_t delay)
    {
        UiWayland* ui = reinterpret_cast<UiWayland*>(data);
        ui->xkb.setup_repeat(rate, delay);
    }

    static void on_keyboard_keymap(void* data, struct wl_keyboard*, uint32_t,
                                   int32_t fd, uint32_t size)
    {
        UiWayland* ui = reinterpret_cast<UiWayland*>(data);
        ui->xkb.set_mapping(fd, size);
        close(fd);
    }

    static void on_keyboard_key(void* data, struct wl_keyboard*, uint32_t,
                                uint32_t, uint32_t key, uint32_t state)
    {
        UiWayland* ui = reinterpret_cast<UiWayland*>(data);

        if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
            ui->xkb.stop_repeat();
        } else if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
            key += 8;
            const xkb_keysym_t keysym = ui->xkb.get_keysym(key);
            if (keysym != XKB_KEY_NoSymbol) {
                ui->event_handler(
                    { Ui::Event::Type::KeyPress,
                      { .key = { keysym, ui->xkb.get_modifiers() } } });
                ui->xkb.start_repeat(key);
            }
        }
    }

    static constexpr const wl_keyboard_listener keyboard_listener = {
        .keymap = on_keyboard_keymap,
        .enter = on_keyboard_enter,
        .leave = on_keyboard_leave,
        .key = on_keyboard_key,
        .modifiers = on_keyboard_modifiers,
        .repeat_info = on_keyboard_repeat_info,
    };

    /***************************************************************************
     * Pointer handlers
     **************************************************************************/
    static void on_pointer_enter(void* data, struct wl_pointer*, uint32_t,
                                 struct wl_surface*, wl_fixed_t, wl_fixed_t)
    {
        UiWayland* ui = reinterpret_cast<UiWayland*>(data);
        ui->set_cursor(Ui::CursorShape::Default);
    }

    static void on_pointer_leave(void*, struct wl_pointer*, uint32_t,
                                 struct wl_surface*)
    {
    }

    static void on_pointer_motion(void* data, struct wl_pointer*, uint32_t,
                                  wl_fixed_t surface_x, wl_fixed_t surface_y)
    {
        UiWayland* ui = reinterpret_cast<UiWayland*>(data);

        const double scale = ui->get_scale();
        ui->mouse_x = wl_fixed_to_int(surface_x * scale);
        ui->mouse_y = wl_fixed_to_int(surface_y * scale);

        if (ui->pointer_shape == Ui::CursorShape::Hide) {
            ui->set_cursor(Ui::CursorShape::Default); // restore cursor
        }

        ui->event_handler(
            { Ui::Event::Type::MouseMove,
              { .mouse = { ui->mouse_buttons, ui->xkb.get_modifiers(),
                           ui->mouse_x, ui->mouse_y } } });
    }

    static void on_pointer_button(void* data, struct wl_pointer*, uint32_t,
                                  uint32_t, uint32_t button, uint32_t state)
    {
        UiWayland* ui = reinterpret_cast<UiWayland*>(data);

        const InputMouse::mouse_btn_t btn = InputMouse::to_button(button);
        if (btn == InputMouse::NONE) {
            return; // unknown button
        }

        if (state == WL_POINTER_BUTTON_STATE_RELEASED) {
            ui->mouse_buttons &= ~btn;
            ui->set_cursor(Ui::CursorShape::Default);
        } else {
            ui->mouse_buttons |= btn;
            ui->event_handler(
                { Ui::Event::Type::MouseClick,
                  { .mouse = { ui->mouse_buttons, ui->xkb.get_modifiers(),
                               ui->mouse_x, ui->mouse_y } } });
        }
    }

    static void on_pointer_axis(void* data, struct wl_pointer*, uint32_t,
                                uint32_t axis, wl_fixed_t value)
    {
        UiWayland* ui = reinterpret_cast<UiWayland*>(data);

        InputMouse::mouse_btn_t btn;
        const bool incr = value > 0;
        if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
            btn = incr ? InputMouse::SCROLL_RIGHT : InputMouse::SCROLL_LEFT;
        } else {
            btn = incr ? InputMouse::SCROLL_DOWN : InputMouse::SCROLL_UP;
        }
        btn |= ui->mouse_buttons;

        ui->event_handler({ Ui::Event::Type::MouseClick,
                            { .mouse = { btn, ui->xkb.get_modifiers(),
                                         ui->mouse_x, ui->mouse_y } } });
    }

    static constexpr const wl_pointer_listener pointer_listener = {
        .enter = on_pointer_enter,
        .leave = on_pointer_leave,
        .motion = on_pointer_motion,
        .button = on_pointer_button,
        .axis = on_pointer_axis,
        .frame = nullptr,
        .axis_source = nullptr,
        .axis_stop = nullptr,
        .axis_discrete = nullptr,
        .axis_value120 = nullptr,
        .axis_relative_direction = nullptr,
    };

    /***************************************************************************
     * Idle handlers
     **************************************************************************/
    static void on_idle_begin(void* data, struct ext_idle_notification_v1*)
    {
        UiWayland* ui = reinterpret_cast<UiWayland*>(data);
        ui->set_cursor(Ui::CursorShape::Hide);
    }

    static void on_idle_end(void*, struct ext_idle_notification_v1*) { }

    static constexpr const ext_idle_notification_v1_listener idle_listener = {
        .idled = on_idle_begin,
        .resumed = on_idle_end,
    };

    /***************************************************************************
     * Fractional scale handlers
     **************************************************************************/
    static void handle_scale(void* data, struct wp_fractional_scale_v1*,
                             uint32_t factor)
    {
        UiWayland* ui = reinterpret_cast<UiWayland*>(data);
        if (ui->scale != factor) {
            ui->scale = factor;
            ui->wnd_buffer.realloc(ui->wl.shm, ui->get_width(),
                                   ui->get_height());
            ui->event_handler({ Ui::Event::Type::WindowRescale, {} });
            ui->event_handler({ Ui::Event::Type::WindowResize, {} });
            ui->event_handler({ Ui::Event::Type::WindowRedraw, {} });
        }
    }

    static constexpr const wp_fractional_scale_v1_listener scale_listener = {
        .preferred_scale = handle_scale,
    };

    /***************************************************************************
     * Seat handlers
     **************************************************************************/
    static void on_seat_name(void*, struct wl_seat*, const char*) { }

    static void on_seat_capabilities(void* data, struct wl_seat* seat,
                                     uint32_t cap)
    {
        UiWayland* ui = reinterpret_cast<UiWayland*>(data);

        if (cap & WL_SEAT_CAPABILITY_KEYBOARD) {
            ui->wl.keyboard = wl_seat_get_keyboard(seat);
            wl_keyboard_add_listener(ui->wl.keyboard, &keyboard_listener, ui);
        } else if (ui->wl.keyboard) {
            wl_keyboard_destroy(ui->wl.keyboard);
            ui->wl.keyboard = nullptr;
        }

        if (cap & WL_SEAT_CAPABILITY_POINTER) {
            ui->wl.pointer = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(ui->wl.pointer, &pointer_listener, ui);
        } else if (ui->wl.pointer) {
            wl_pointer_destroy(ui->wl.pointer);
            ui->wl.pointer = nullptr;
        }

        // register idle listener
        if (ui->wl.idle_mgr) {
            ui->wl.idle = ext_idle_notifier_v1_get_idle_notification(
                ui->wl.idle_mgr, ui->cursor_hide, ui->wl.seat);
            ext_idle_notification_v1_add_listener(ui->wl.idle, &idle_listener,
                                                  ui);
        }
    }

    static constexpr const wl_seat_listener seat_listener = {
        .capabilities = on_seat_capabilities,
        .name = on_seat_name,
    };

    /***************************************************************************
     * Frame handlers
     **************************************************************************/
    static void on_frame_done(void* data, struct wl_callback*, uint32_t)
    {
        UiWayland* ui = reinterpret_cast<UiWayland*>(data);
        wl_callback_destroy(ui->wl.callback);
        ui->wl.callback.ptr = nullptr;
        ui->wl.callback = wl_surface_frame(ui->wl.surface);
        wl_callback_add_listener(ui->wl.callback, &frame_listener, ui);

        ui->frame_mutex.unlock();
    }

    static constexpr const wl_callback_listener frame_listener = {
        .done = on_frame_done
    };

    /***************************************************************************
     * XDG handlers
     **************************************************************************/
    static void handle_xdg_toplevel_configure(void* data, struct xdg_toplevel*,
                                              int32_t width, int32_t height,
                                              struct wl_array*)
    {
        if (width > 0 && height > 0) {
            UiWayland* ui = reinterpret_cast<UiWayland*>(data);
            ui->width = width;
            ui->height = height;
        }
    }

    static void handle_xdg_toplevel_close(void* data, struct xdg_toplevel*)
    {
        UiWayland* ui = reinterpret_cast<UiWayland*>(data);
        ui->event_handler({ Ui::Event::Type::WindowClose, {} });
    }

    static constexpr const xdg_toplevel_listener xdgtoplvl_listener = {
        .configure = handle_xdg_toplevel_configure,
        .close = handle_xdg_toplevel_close,
        .configure_bounds = nullptr,
        .wm_capabilities = nullptr,
    };

    static void on_xdg_surface_configure(void* data,
                                         struct xdg_surface* surface,
                                         uint32_t serial)
    {
        xdg_surface_ack_configure(surface, serial);

        UiWayland* ui = reinterpret_cast<UiWayland*>(data);

        ui->wnd_buffer.realloc(ui->wl.shm, ui->get_width(), ui->get_height());
        if (ui->wl.viewport) {
            wp_viewport_set_destination(ui->wl.viewport, ui->width, ui->height);
        }

        ui->event_handler({ Ui::Event::Type::WindowResize, {} });
        ui->event_handler({ Ui::Event::Type::WindowRedraw, {} });
    }

    static constexpr const xdg_surface_listener xdgsurface_listener = {
        .configure = on_xdg_surface_configure
    };

    static void on_xdg_ping(void*, struct xdg_wm_base* base, uint32_t serial)
    {
        xdg_wm_base_pong(base, serial);
    }

    static constexpr const xdg_wm_base_listener xdgbase_listener = {
        .ping = on_xdg_ping
    };

    /***************************************************************************
     * Registry handlers
     **************************************************************************/
    static void on_registry_global(void* data, struct wl_registry* registry,
                                   uint32_t name, const char* interface,
                                   uint32_t /* version */)
    {
        UiWayland* ui = reinterpret_cast<UiWayland*>(data);

        if (strcmp(interface, wl_compositor_interface.name) == 0) {
            // wayland compositor
            ui->wl.compositor.bind(registry, name,
                                   WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION);
        } else if (strcmp(interface, wl_shm_interface.name) == 0) {
            // wayland shared memory
            ui->wl.shm.bind(registry, name,
                            WL_SHM_POOL_CREATE_BUFFER_SINCE_VERSION);
        } else if (strcmp(interface, wl_seat_interface.name) == 0) {
            // seat (keyboard and mouse)
            ui->wl.seat.bind(registry, name,
                             WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION);
            wl_seat_add_listener(ui->wl.seat, &seat_listener, data);
        } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
            // xdg (app window)
            ui->wl.xwmbase.bind(registry, name, XDG_WM_BASE_PING_SINCE_VERSION);
            xdg_wm_base_add_listener(ui->wl.xwmbase, &xdgbase_listener, data);
        } else if (strcmp(interface, wp_viewporter_interface.name) == 0) {
            // viewport (fractional scale output)
            ui->wl.viewporter.bind(registry, name,
                                   WP_VIEWPORTER_GET_VIEWPORT_SINCE_VERSION);
        } else if (strcmp(interface, ext_idle_notifier_v1_interface.name) ==
                   0) {
            // idle notifier
            ui->wl.idle_mgr.bind(registry, name,
                                 EXT_IDLE_NOTIFIER_V1_GET_IDLE_NOTIFICATION);
        } else if (strcmp(interface,
                          wp_cursor_shape_manager_v1_interface.name) == 0) {
            // cursor shape
            ui->wl.cursor_mgr.bind(
                registry, name,
                WP_CURSOR_SHAPE_MANAGER_V1_GET_POINTER_SINCE_VERSION);
        } else if (strcmp(interface,
                          wp_content_type_manager_v1_interface.name) == 0) {
            // content type
            ui->wl.ctype_mgr.bind(
                registry, name,
                WP_CONTENT_TYPE_V1_SET_CONTENT_TYPE_SINCE_VERSION);
        } else if (strcmp(interface,
                          wp_fractional_scale_manager_v1_interface.name) == 0) {
            // fractional scale manager
            ui->wl.scale_mgr.bind(
                registry, name,
                WP_FRACTIONAL_SCALE_V1_PREFERRED_SCALE_SINCE_VERSION);
        } else if (strcmp(interface,
                          zxdg_decoration_manager_v1_interface.name) == 0) {
            // server side window decoration
            ui->wl.decor_mgr.bind(
                registry, name,
                ZXDG_DECORATION_MANAGER_V1_GET_TOPLEVEL_DECORATION_SINCE_VERSION);
        }
    }

    static void on_registry_remove(void*, struct wl_registry*, uint32_t) { }

    static constexpr const wl_registry_listener registry_listener = {
        .global = on_registry_global,
        .global_remove = on_registry_remove,
    };
};

WaylandBuffer::~WaylandBuffer()
{
    destroy();
}

bool WaylandBuffer::realloc(struct wl_shm* shm, size_t width, size_t height)
{
    if (width == pm.width() && height == pm.height()) {
        return true; // reuse existing
    }

    std::lock_guard lock(mutex);

    destroy();

    const size_t stride = width * sizeof(argb_t);
    const size_t size = height * stride;

    // generate unique file name
    static size_t counter = 0;
    const std::string path =
        std::format("/swayimg_{:x}_{:x}", getpid(), ++counter);

    // open shared mem
    const Fd fd = shm_open(path.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd == -1) {
        Log::error(errno, "Unable to create shared file {}", path);
        return false;
    }
    shm_unlink(path.c_str());

    // set shared memory size
    if (ftruncate(fd, size) == -1) {
        Log::error(errno, "Unable to truncate shared file {}", path);
        return false;
    }

    // get data pointer of the shared mem
    void* data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        Log::error(errno, "Unable to map shared file {}", path);
        return false;
    }

    // create wayland buffer
    wl_shm_pool* pool = wl_shm_create_pool(shm, fd, size);
    if (!pool) {
        Log::error("Unable create wayland shared poll");
        munmap(data, size);
        return false;
    }
    buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride,
                                       WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);

    pm.attach(Pixmap::ARGB, width, height, data);

    return true;
}

void WaylandBuffer::destroy()
{
    if (buffer) {
        const size_t size = pm.stride() * pm.height();
        munmap(pm.ptr(0, 0), size);

        wl_buffer_destroy(buffer);

        buffer = nullptr;
        pm.free();
    }
}

UiWayland::UiWayland(const EventHandler& handler)
    : Ui(handler)
{
}

bool UiWayland::initialize(const std::string& app_id)
{
    wl.display = wl_display_connect(nullptr);
    if (!wl.display) {
        Log::error(errno, "Failed to open wayland display");
        return false;
    }

    wl.registry = wl_display_get_registry(wl.display);
    if (!wl.registry) {
        Log::error("Failed to open wayland registry");
        return false;
    }

    wl_registry_add_listener(wl.registry, &WaylandHandler::registry_listener,
                             this);
    wl_display_roundtrip(wl.display);

    wl.surface = wl_compositor_create_surface(wl.compositor);
    if (!wl.surface) {
        Log::error("Failed to create wayland surface");
        return false;
    }
    wl.callback = wl_surface_frame(wl.surface);
    wl_callback_add_listener(wl.callback, &WaylandHandler::frame_listener,
                             this);

    assert(wl.xwmbase);
    wl.xsurface = xdg_wm_base_get_xdg_surface(wl.xwmbase, wl.surface);
    if (!wl.xsurface) {
        Log::error("Failed to create xdg surface");
        return false;
    }

    xdg_surface_add_listener(wl.xsurface, &WaylandHandler::xdgsurface_listener,
                             this);
    wl.xtoplevel = xdg_surface_get_toplevel(wl.xsurface);
    if (!wl.xtoplevel) {
        Log::error("Failed to get xdg top level");
        return false;
    }
    xdg_toplevel_add_listener(wl.xtoplevel, &WaylandHandler::xdgtoplvl_listener,
                              this);
    xdg_toplevel_set_app_id(wl.xtoplevel, app_id.c_str());
    xdg_toplevel_set_title(wl.xtoplevel, app_id.c_str());
    if (fullscreen) {
        xdg_toplevel_set_fullscreen(wl.xtoplevel, nullptr);
    }

    if (wl.scale_mgr) {
        wl.scale = wp_fractional_scale_manager_v1_get_fractional_scale(
            wl.scale_mgr, wl.surface);
        wp_fractional_scale_v1_add_listener(
            wl.scale, &WaylandHandler::scale_listener, this);
    }
    if (wl.viewporter) {
        wl.viewport = wp_viewporter_get_viewport(wl.viewporter, wl.surface);
    }

    if (wl.ctype_mgr) {
        wl.ctype = wp_content_type_manager_v1_get_surface_content_type(
            wl.ctype_mgr, wl.surface);
        wp_content_type_v1_set_content_type(wl.ctype,
                                            WP_CONTENT_TYPE_V1_TYPE_PHOTO);
    }

    if (decoration) {
        if (!wl.decor_mgr) {
            Log::warning("Decoration manager is not available");
        } else {
            wl.decor = zxdg_decoration_manager_v1_get_toplevel_decoration(
                wl.decor_mgr, wl.xtoplevel);
            if (wl.decor) {
                zxdg_toplevel_decoration_v1_set_mode(
                    wl.decor, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
            } else {
                Log::warning("Failed to set top level decoration");
            }
        }
    }

    wl_surface_commit(wl.surface);

    return true;
}

void UiWayland::run()
{
    thread = std::thread([this] {
        pollfd fds[] = {
            {
             .fd = stop_event,
             .events = POLLIN,
             .revents = 0,
             },
            {
             .fd = wl_display_get_fd(wl.display),
             .events = POLLIN,
             .revents = 0,
             },
            {
             .fd = flush_event,
             .events = POLLIN,
             .revents = 0,
             },
            {
             .fd = xkb.repeat_fd(),
             .events = POLLIN,
             .revents = 0,
             },
        };

        // main event loop
        while (true) {
            // prepare to read wayland events
            while (wl_display_prepare_read(wl.display) != 0) {
                wl_display_dispatch_pending(wl.display);
            }
            wl_display_flush(wl.display);

            // poll events
            if (poll(fds, sizeof(fds) / sizeof(fds[0]), -1) <= 0) {
                wl_display_cancel_read(wl.display);
                continue;
            }

            if (fds[0].revents & POLLIN) {
                break; // stop event
            }

            // read and handle wayland events
            if (fds[1].revents & POLLIN) {
                wl_display_read_events(wl.display);
                wl_display_dispatch_pending(wl.display);
            } else {
                wl_display_cancel_read(wl.display);
            }

            // buffer flush
            if (fds[2].revents & POLLIN) {
                flush_event.reset();
                wl_surface_attach(wl.surface, wnd_buffer, 0, 0);
                wl_surface_damage_buffer(wl.surface, 0, 0,
                                         wnd_buffer.pm.width(),
                                         wnd_buffer.pm.height());
                wl_surface_commit(wl.surface);
            }

            // read and handle key repeat events from timer
            if (fds[3].revents & POLLIN) {
                const auto [key, repeat] = xkb.get_repeat();
                const keymod_t km = xkb.get_modifiers();
                for (size_t i = 0; i < repeat; ++i) {
                    event_handler(
                        { Ui::Event::Type::KeyPress, { .key = { key, km } } });
                }
            }
        }
    });
}

void UiWayland::stop()
{
    if (thread.joinable()) {
        stop_event.set();
        thread.join();
    }
}

void UiWayland::set_title(const char* title)
{
    xdg_toplevel_set_title(wl.xtoplevel, title);
}

void UiWayland::set_cursor(CursorShape shape)
{
    if (!wl.pointer || !wl.cursor_mgr) {
        return; // not supported
    }

    pointer_shape = shape;

    wp_cursor_shape_device_v1_shape wlshape =
        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
    switch (shape) {
        case CursorShape::Default:
            break;
        case CursorShape::Drag:
            wlshape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRABBING;
            break;
        case CursorShape::Hide:
            wl_pointer_set_cursor(wl.pointer, 0, nullptr, 0, 0);
            return;
    }

    wp_cursor_shape_device_v1* dev =
        wp_cursor_shape_manager_v1_get_pointer(wl.cursor_mgr, wl.pointer);
    if (dev) {
        wp_cursor_shape_device_v1_set_shape(dev, 0, wlshape);
        wp_cursor_shape_device_v1_destroy(dev);
    }
}

void UiWayland::set_ctype(ContentType type)
{
    if (!wl.ctype) {
        return; // not supported
    }

    uint32_t content_type = WP_CONTENT_TYPE_V1_TYPE_PHOTO;
    switch (type) {
        case ContentType::Static:
            break;
        case ContentType::Animation:
            content_type = WP_CONTENT_TYPE_V1_TYPE_VIDEO;
            break;
    }
    wp_content_type_v1_set_content_type(wl.ctype, content_type);
}

void UiWayland::toggle_fullscreen()
{
    fullscreen = !fullscreen;

    if (wl.xtoplevel) {
        if (fullscreen) {
            xdg_toplevel_set_fullscreen(wl.xtoplevel, nullptr);
        } else {
            xdg_toplevel_unset_fullscreen(wl.xtoplevel);
        }
    }
}

double UiWayland::get_scale()
{
    return static_cast<double>(scale) / FRACTION_SCALE_DEN;
}

size_t UiWayland::get_width()
{
    return get_scale() * width;
}

size_t UiWayland::get_height()
{
    return get_scale() * height;
}

Pixmap& UiWayland::lock_surface()
{
    if (wnd_buffer) {
        frame_mutex.lock();
        wnd_buffer.mutex.lock();
    }
    return wnd_buffer.pm;
}

void UiWayland::commit_surface()
{
    if (wnd_buffer) {
        wnd_buffer.mutex.unlock();
        flush_event.set();
    }
}
