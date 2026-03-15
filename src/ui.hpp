// SPDX-License-Identifier: MIT
// UI generic interface.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "pixmap.hpp"

class Ui {
public:
    /** Cursor shapes. */
    enum class CursorShape : uint8_t {
        Default,
        Drag,
        Copy,
        Hide,
    };

    /** Content types. */
    enum class ContentType : uint8_t {
        Static,
        Animation,
    };

    virtual ~Ui() = default;

    /**
     * Run user interface.
     */
    virtual void run() {}

    /**
     * Stop user interface.
     */
    virtual void stop() {}

    /**
     * Set window title.
     * @param title title to set
     */
    virtual void set_title(const char* /* title */) {}

    /**
     * Set mouse pointer shape.
     * @param shape cursor shape to set
     */
    virtual void set_cursor(CursorShape /* shape */) {}

    /**
     * Set surface content type.
     * @param ctype content type to set
     */
    virtual void set_ctype(ContentType /* type */) {}

    /**
     * Toggle full screen mode.
     * @return true if full screen mode enabled
     */
    virtual bool toggle_fullscreen() { return false; }

    /**
     * Get window size.
     * @return window size in pixels
     */
    virtual Size get_window_size() = 0;

    /**
     * Set window size.
     * @param size window size in pixels
     */
    virtual void set_window_size(const Size& /*size*/) {}

    /**
     * Get mouse pointer position within the window.
     * @return mouse pointer coordinates
     */
    virtual Point get_mouse() { return {}; }

    /**
     * Begin window redraw procedure.
     * @return window surface pixmap
     */
    virtual Pixmap& lock_surface() = 0;

    /**
     * Finalize window redraw procedure.
     */
    virtual void commit_surface() = 0;
};
