// SPDX-License-Identifier: MIT
// Integration with Wayland compositor (Sway and Hyprland only).
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stdbool.h>
#include <sys/types.h>

/** Position and size of a window. */
struct wndrect {
    ssize_t x, y;
    size_t width, height;
};

/**
 * Get geometry of currently focused window.
 * @param wnd geometry of currently focused window
 * @return true if operation completed successfully
 */
bool compositor_get_focus(struct wndrect* wnd);

/**
 * Set rules to create overlay window.
 * @param wnd geometry of app's window
 * @param app_id application class name
 * @return true if operation completed successfully
 */
bool compositor_overlay(const struct wndrect* wnd, char** app_id);
