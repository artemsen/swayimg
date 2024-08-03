// SPDX-License-Identifier: MIT
// Image viewer application: main loop and event handler.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "keybind.h"
#include "image.h"

// Configuration parameters
#define APP_CFG_SECTION  "general"
#define APP_CFG_MODE     "mode"
#define APP_CFG_POSITION "position"
#define APP_CFG_SIZE     "size"
#define APP_CFG_APP_ID   "app_id"
#define APP_MODE_VIEWER  "viewer"
#define APP_MODE_GALLERY "gallery"
#define APP_FROM_PARENT  "parent"
#define APP_FROM_IMAGE   "image"
#define APP_FULLSCREEN   "fullscreen"

/**
 * Handler of the fd poll events.
 * @param data user data
 */
typedef void (*fd_callback)(void* data);

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
 * @param data user defined data to pass to callback
 */
void app_watch(int fd, fd_callback cb, void* data);

/**
 * Run application.
 * @return true if application was closed by user, false on errors
 */
bool app_run(void);

/**
 * Handler of external event: application exit request.
 * @param rc result (error) code to set
 */
void app_exit(int rc);

/**
 * Switch mode (viewer/gallery).
 * @param index index of the current image
 */
void app_switch_mode(size_t index);

/**
 * Get active mode.
 * @return true if current mode is viewer, false for gallery
 */
bool app_is_viewer(void);

/**
 * Handler of external event: reload image / reset state.
 */
void app_reload(void);

/**
 * Handler of external event: redraw window.
 */
void app_redraw(void);

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
 * Handler of image loading completion (background thread loader).
 * @param image loaded image instance, NULL if load error
 * @param index index of the image in the image list
 */
void app_on_load(struct image* image, size_t index);

/**
 * Execute system command for the specified image.
 * @param expr command expression
 * @param path file path to substitute into expression
 */
void app_execute(const char* expr, const char* path);
