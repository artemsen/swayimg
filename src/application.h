// SPDX-License-Identifier: MIT
// Image viewer application: main loop and event handler.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "keybind.h"
#include "pixmap.h"

// Configuration parameters
#define APP_CFG_APP_ID "app_id"

/** Handler of the fd poll events. */
typedef void (*fd_callback)(void);

/**
 * Create global application context.
 */
void app_create(void);

/**
 * Destroy global application context.
 */
void app_destroy(void);

/**
 * Initialize global application context.
 * @param sources list of sources
 * @param num number of sources in the list
 * @return true if application initialized successfully
 */
bool app_init(const char** sources, size_t num);

/**
 * Add file descriptor for polling in main loop.
 * @param fd file descriptor for polling
 * @param cb callback function
 */
void app_watch(int fd, fd_callback cb);

/**
 * Run application.
 * @return true if application was closed by user, false on errors
 */
bool app_run(void);

/**
 * Handler of external event: reload image / reset state.
 */
void app_on_reload(void);

/**
 * Handler of external event: redraw window.
 */
void app_on_redraw(void);

/**
 * Handler of external event: window resized.
 */
void app_on_resize(void);

/**
 * Handler of external event: key/mouse press.
 * @param key code of key pressed
 * @param mods key modifiers (ctrl/alt/shift)
 */
void app_on_keyboard(xkb_keysym_t key, uint8_t mods);

/**
 * Handler of external event: mouse/touch drag.
 * @param dx,dy delta between old and new position
 */
void app_on_drag(int dx, int dy);

/**
 * Handler of external event: application exit request.
 * @param rc result (error) code to set
 */
void app_on_exit(int rc);
