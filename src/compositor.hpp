// SPDX-License-Identifier: MIT
// Integration with Wayland compositors (Sway and Hyprland only).
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "geometry.hpp"

#include <cstdint>
#include <string>

class Compositor {
public:
    /** Compositor type. */
    enum Type : uint8_t {
        None,
        Sway,
        Hyprland,
    };

    Compositor();

    /**
     * Get currently focused window position and size.
     * @return currently focused window position and size
     */
    Rectangle get_focus() const;

    /**
     * Set rules to create overlay (floating) window.
     * @param wnd preferable geometry of app's window
     * @param app_id application id (class name)
     */
    void set_overlay(const Rectangle& wnd, std::string& app_id) const;

    Type type; ///< Currently running compositor
};
