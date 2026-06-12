// SPDX-License-Identifier: MIT
// Startup parameters.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "appmode.hpp"
#include "geometry.hpp"

#include <filesystem>
#include <vector>

/** Lockable parameter. */
template <typename T> class Lockable {
public:
    operator const T&() const { return value; }

    Lockable& operator=(const T& val)
    {
        if (!locked) {
            value = val;
        }
        return *this;
    }

    /**
     * Get value.
     * @return value
     */
    [[nodiscard]] const T& get() const { return value; }

    /**
     * Set value.
     * @param val value to set
     */
    void set(const T& val)
    {
        if (!locked) {
            value = val;
        }
    }

    /**
     * Set and lock value.
     * @param val value to set
     */
    void lock(const T& val)
    {
        set(val);
        locked = true;
    }

    /**
     * Unlock value.
     * @return value reference
     */
    T& unlock()
    {
        locked = false;
        return value;
    }

private:
    T value;
    bool locked = false;
};

/** Startup parameters. */
struct StartupParams {
    std::vector<std::filesystem::path> sources; ///< Image list

    std::filesystem::path config; ///< Lua config file to load
    std::string lua_exec;         ///< Lua code to execute at start

    Lockable<std::string> app_id; ///< Window class name

    Lockable<AppMode::Type> mode; ///< Initial mode

    Lockable<bool> fullscreen; ///< Full screen mode

    Lockable<Size> wnd_size; ///< Window size
    Lockable<Point> wnd_pos; ///< Window position

    InputMouse dnd; ///< Mouse used for drag-and-drop

    bool use_overlay;     ///< Use overlay mode
    bool decoration;      ///< Window decoration
    uint32_t cursor_hide; ///< Cursor hide time
};
