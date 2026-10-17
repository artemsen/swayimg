// SPDX-License-Identifier: MIT
// UI generic interface.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "pixmap.hpp"

#include <string>

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
     * Make a window title safe to hand over to the compositor. The title
     * contains a file name, and compositors pass it on to bars, task lists
     * and scripts. Control characters (C0, DEL and C1) and Unicode
     * bidirectional formatting characters (which can make "gnp.exe" look
     * like "exe.png") are replaced with '?', the rest of UTF-8 is kept.
     * @param title raw title
     * @return safe title
     */
    static std::string sanitize_title(const std::string& title)
    {
        const size_t len = title.length();
        std::string result;
        result.reserve(len);

        for (size_t i = 0; i < len; ++i) {
            const auto b0 = static_cast<uint8_t>(title[i]);
            const uint8_t b1 =
                i + 1 < len ? static_cast<uint8_t>(title[i + 1]) : 0;
            const uint8_t b2 =
                i + 2 < len ? static_cast<uint8_t>(title[i + 2]) : 0;

            // length of the unsafe sequence at this position, if any
            size_t unsafe = 0;
            if (b0 < ' ' || b0 == 0x7f) {
                unsafe = 1; // C0 and DEL
            } else if ((b0 == 0xc2 && b1 >= 0x80 && b1 <= 0x9f) /* C1 */ ||
                       (b0 == 0xd8 && b1 == 0x9c) /* U+061C */) {
                unsafe = 2;
            } else if (b0 == 0xe2) {
                const bool bidi =
                    // U+200E, U+200F, U+202A-U+202E
                    (b1 == 0x80 &&
                     (b2 == 0x8e || b2 == 0x8f ||
                      (b2 >= 0xaa && b2 <= 0xae))) ||
                    // U+2066-U+2069
                    (b1 == 0x81 && b2 >= 0xa6 && b2 <= 0xa9);
                unsafe = bidi ? 3 : 0;
            }

            if (unsafe) {
                result += '?';
                i += unsafe - 1;
            } else {
                result += static_cast<char>(b0);
            }
        }

        return result;
    }

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
    virtual void set_cursor(const CursorShape /* shape */) {}

    /**
     * Set surface content type.
     * @param ctype content type to set
     */
    virtual void set_ctype(const ContentType /* type */) {}

    /**
     * Enable/disable full screen mode.
     * @param enable true to enable full screen mode
     */
    virtual void set_fullscreen(const bool /* enable */) {}

    /**
     * Get current full screen mode.
     * @return true if full screen mode enabled
     */
    [[nodiscard]] virtual bool get_fullscreen() const { return true; }

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
     * @return window surface pixmap, nullptr if window not yet created
     */
    virtual Pixmap* lock_surface() = 0;

    /**
     * Finalize window redraw procedure.
     */
    virtual void commit_surface() = 0;
};
