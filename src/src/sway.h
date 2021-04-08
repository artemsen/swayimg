// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stdbool.h>

/** Rectangle description. */
struct rect {
    int x;
    int y;
    int width;
    int height;
};

/**
 * Connect to Sway.
 * @return IPC context, -1 if error
 */
int sway_connect(void);

/**
 * Disconnect IPC channel.
 * @param[in] ipc IPC context
 */
void sway_disconnect(int ipc);

/**
 * Get geometry for currently focused window.
 * @param[in] ipc IPC context
 * @param[out] wnd geometry of currently focused window
 * @param[out] fullscreen current full screen mode
 * @return true if operation completed successfully
 */
bool sway_current(int ipc, struct rect* wnd, bool* fullscreen);

/**
 * Add rules for Sway for application's window:
 * 1. Enable floating mode;
 * 2. Set initial position.
 *
 * @param[in] ipc IPC context
 * @param[in] app application Id
 * @param[in] x horizontal window position relative to current workspace
 * @param[in] v vertical window position relative to current workspace
 * @return true if operation completed successfully
 */
bool sway_add_rules(int ipc, const char* app, int x, int y);
