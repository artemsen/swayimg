// SPDX-License-Identifier: MIT
// Integration with Wayland compositors (Sway and Hyprland only).
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "geometry.hpp"

#include <string>

namespace Compositor {

/**
 * Set rules to create overlay window.
 * @param wnd preferable geometry of app's window, if rect is not valid, then
 *            it will be filled from parent window
 * @param app_id application id (class name), can be modified
 * @return true if rules were set
 */
bool setup_overlay(Rectangle& wnd, std::string& app_id);

};
