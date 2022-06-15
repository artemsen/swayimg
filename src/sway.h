// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "canvas.h"

#include <stdbool.h>

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
bool sway_current(int ipc, rect_t* wnd, bool* fullscreen);

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

/**
 * Add a Sway rule to move the application's window to another output
 *
 * @param[in] ipc IPC context
 * @param[in] app application Id
 * @param[in] output name from `swaymsg -t get_outputs`
 * @return true if operation completed successfully
 */
bool sway_move_to_output(int ipc, const char* app, const char* output);
