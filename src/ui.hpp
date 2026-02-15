// SPDX-License-Identifier: MIT
// UI generic interface.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "input.hpp"
#include "pixmap.hpp"

class Ui {
public:
    /** UI event. */
    struct Event {
        /** Event types. */
        enum class Type : uint8_t {
            WindowClose,   ///< Window closed
            WindowResize,  ///< Window resized
            WindowRedraw,  ///< Window needs to be redrawn
            WindowRescale, ///< Window scale changed
            KeyPress,      ///< Key pressed
            MouseMove,     ///< Mouse moved
            MouseClick,    ///< Mouse button pressed
        };

        Type type;

        union Data {
            InputKeyboard key;
            InputMouse mouse;
        } data;
    };
    using EventHandler = std::function<void(Event)>;

    /** Cursor shapes. */
    enum class CursorShape : uint8_t {
        Default,
        Drag,
        Hide,
    };

    /** Content types. */
    enum class ContentType : uint8_t {
        Static,
        Animation,
    };

    Ui(const EventHandler& eh)
        : event_handler(eh)
    {
    }

    virtual ~Ui() = default;

    /**
     * Run user interface.
     */
    virtual void run() { }

    /**
     * Stop user interface.
     */
    virtual void stop() { }

    /**
     * Set window title.
     * @param title title to set
     */
    virtual void set_title(const char* /* title */) { }

    /**
     * Set mouse pointer shape.
     * @param shape cursor shape to set
     */
    virtual void set_cursor(CursorShape /* shape */) { }

    /**
     * Set surface content type.
     * @param ctype content type to set
     */
    virtual void set_ctype(ContentType /* type */) { }

    /**
     * Toggle full screen mode.
     */
    virtual void toggle_fullscreen() { }

    /**
     * Get window scale factor.
     * @return window scale factor
     */
    virtual double get_scale() { return 1; }

    /**
     * Get window width.
     * @return window width in pixels
     */
    virtual size_t get_width() = 0;

    /**
     * Get window height.
     * @return window height in pixels
     */
    virtual size_t get_height() = 0;

    /**
     * Begin window redraw procedure.
     * @return window surface pixmap
     */
    virtual Pixmap& lock_surface() = 0;

    /**
     * Finalize window redraw procedure.
     */
    virtual void commit_surface() = 0;

protected:
    const EventHandler event_handler; ///< UI event handler
};
