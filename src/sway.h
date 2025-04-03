// SPDX-License-Identifier: MIT
// Integration with Sway WM.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#define INVALID_SWAY_IPC -1

/** Position and size of a window. */
struct wndrect {
    ssize_t x;
    ssize_t y;
    size_t width;
    size_t height;
};

/**
 * Connect to Sway.
 * @return IPC context, INVALID_SWAY_IPC if error
 */
int sway_connect(void);

/**
 * Disconnect IPC channel.
 * @param ipc IPC context
 */
void sway_disconnect(int ipc);

/**
 * Get geometry for currently focused window.
 * @param ipc IPC context
 * @param wnd geometry of currently focused window
 * @param border size of window border in pixels
 * @param fullscreen current full screen mode
 * @return true if operation completed successfully
 */
bool sway_current(int ipc, struct wndrect* wnd, int* border, bool* fullscreen);

/**
 * Add rules for Sway for application's window:
 * 1. Enable floating mode;
 * 2. Set initial position.
 *
 * @param ipc IPC context
 * @param x horizontal window position
 * @param v vertical window position
 * @return true if operation completed successfully
 */
bool sway_add_rules(int ipc, int x, int y);
