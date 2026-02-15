// SPDX-License-Identifier: MIT
// Wayland based user interface.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "fdevent.hpp"
#include "ui.hpp"
#include "xkb.hpp"

#include <content-type-v1-client-protocol.h>
#include <cursor-shape-v1-client-protocol.h>
#include <ext-idle-notify-v1-client-protocol.h>
#include <fractional-scale-v1-client-protocol.h>
#include <viewporter-client-protocol.h>
#include <xdg-decoration-unstable-v1-client-protocol.h>
#include <xdg-shell-client-protocol.h>

#include <cassert>
#include <mutex>
#include <thread>

/** Generic Wayland object. */
template <typename T> struct WaylandObject {
public:
    typedef void (*fn_destroy)(T*);

    WaylandObject(fn_destroy fn)
        : destroy(fn)
    {
    }

    ~WaylandObject()
    {
        if (ptr) {
            destroy(ptr);
        }
    }

    operator T*() const { return ptr; };

    fn_destroy destroy;
    T* ptr = nullptr;
};

/** Wayland display object wrapper. */
struct WaylandDisplay : public WaylandObject<wl_display> {
    WaylandDisplay()
        : WaylandObject(wl_display_disconnect)
    {
    }

    WaylandDisplay& operator=(wl_display* object)
    {
        assert(!ptr);
        ptr = object;
        return *this;
    };
};

/** Macro to declare wayland object with RAII. */
#define WLOBJ_DECLARE(T)                                                    \
    struct WaylandObject##T : public WaylandObject<T> {                     \
        WaylandObject##T()                                                  \
            : WaylandObject(T##_destroy)                                    \
        {                                                                   \
        }                                                                   \
                                                                            \
        WaylandObject##T& operator=(T* object)                              \
        {                                                                   \
            assert(!ptr);                                                   \
            ptr = object;                                                   \
            return *this;                                                   \
        }                                                                   \
                                                                            \
        void bind(wl_registry* registry, uint32_t name, uint32_t version)   \
        {                                                                   \
            assert(!ptr);                                                   \
            ptr = reinterpret_cast<T*>(                                     \
                wl_registry_bind(registry, name, &T##_interface, version)); \
        }                                                                   \
    }

/** Wayland window buffer. */
struct WaylandBuffer {
    ~WaylandBuffer();

    /**
     * Reallocate buffer.
     * @param shm wayland shared memory handle
     * @param width,height buffer size in pixels
     * @return true if buffer was allocated
     */
    bool realloc(struct wl_shm* shm, size_t width, size_t height);

    /**
     * Destroy buffer.
     */
    void destroy();

    operator wl_buffer*() { return buffer; }

    std::mutex mutex;            ///< Buffer lock
    wl_buffer* buffer = nullptr; ///< Wayland buffer
    Pixmap pm;                   ///< Pixmap attached to the buffer
};

/** Wayland based user interface. */
class UiWayland : public Ui {
public:
    friend class WaylandHandler;
    UiWayland(const EventHandler& handler);

    /**
     * Initialize Wayland UI.
     * @param app_id application id
     * @return true on success
     */
    bool initialize(const std::string& app_id);

    // Implementation of UI generic interface
    void run() override;
    void stop() override;
    void set_title(const char* title) override;
    void set_cursor(CursorShape shape) override;
    void set_ctype(ContentType type) override;
    void toggle_fullscreen() override;
    double get_scale() override;
    size_t get_width() override;
    size_t get_height() override;
    Pixmap& lock_surface() override;
    void commit_surface() override;

private:
    // Fractional scale denominator (Wayland constant)
    static constexpr const size_t FRACTION_SCALE_DEN = 120;

    std::thread thread;  ///< Wayland event handler thread
    FdEvent stop_event;  ///< Stop signaling event
    FdEvent flush_event; ///< Flush buffer event

    /** Wayland objects. */
    struct WaylandObjects {
        WaylandDisplay display;
        WLOBJ_DECLARE(wl_registry) registry;
        WLOBJ_DECLARE(wl_compositor) compositor;
        WLOBJ_DECLARE(wl_surface) surface;
        WLOBJ_DECLARE(wl_callback) callback;
        WLOBJ_DECLARE(wl_pointer) pointer;
        WLOBJ_DECLARE(wl_keyboard) keyboard;
        WLOBJ_DECLARE(wl_seat) seat;
        WLOBJ_DECLARE(wl_shm) shm;
        WLOBJ_DECLARE(xdg_wm_base) xwmbase;
        WLOBJ_DECLARE(xdg_surface) xsurface;
        WLOBJ_DECLARE(xdg_toplevel) xtoplevel;
        WLOBJ_DECLARE(wp_viewporter) viewporter;
        WLOBJ_DECLARE(wp_viewport) viewport;
        WLOBJ_DECLARE(wp_cursor_shape_manager_v1) cursor_mgr;
        WLOBJ_DECLARE(wp_content_type_manager_v1) ctype_mgr;
        WLOBJ_DECLARE(wp_content_type_v1) ctype;
        WLOBJ_DECLARE(wp_fractional_scale_manager_v1) scale_mgr;
        WLOBJ_DECLARE(wp_fractional_scale_v1) scale;
        WLOBJ_DECLARE(zxdg_decoration_manager_v1) decor_mgr;
        WLOBJ_DECLARE(zxdg_toplevel_decoration_v1) decor;
        WLOBJ_DECLARE(ext_idle_notifier_v1) idle_mgr;
        WLOBJ_DECLARE(ext_idle_notification_v1) idle;
    } wl;

    WaylandBuffer wnd_buffer; ///< Window buffers (double buffering)

    std::mutex frame_mutex; ///< Draw frame lock

    uint32_t scale = FRACTION_SCALE_DEN; ///< Window scale factor (WL format)

    Xkb xkb; ///< X keyboard extension

    CursorShape pointer_shape =
        Ui::CursorShape::Default; ///< Current mouse pointer shape
    InputMouse::mouse_btn_t mouse_buttons =
        InputMouse::NONE; ///< Currently pressed mouse buttons
    size_t mouse_x = 0;   ///< Mouse coordinate X
    size_t mouse_y = 0;   ///< Mouse coordinate Y

public:
    bool decoration = false;     ///< Flag to use server side decoration
    bool fullscreen = false;     ///< Full screen mode
    int32_t width = 1280;        ///< Window width
    int32_t height = 720;        ///< Window height
    uint32_t cursor_hide = 3000; ///< Timeout to hide cursor, 3 sec
};
