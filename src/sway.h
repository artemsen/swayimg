// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stdbool.h>

/** Rectangle descrition. */
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
 * @param[out] rect geometry of currently focused window
 * @return true if operation completed successfully
 */
bool sway_get_focused(int ipc, struct rect* rect);

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
