// SPDX-License-Identifier: MIT
// Image viewer application: main loop and event handler.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "fs.h"
#include "image.h"
#include "keybind.h"

/**
 * Handler of the fd poll events.
 * @param data user data
 */
typedef void (*fd_callback)(void* data);

/**
 * Initialize global application context.
 * @param cfg config instance
 * @param sources list of sources
 * @param num number of sources in the list
 * @return true if application initialized successfully
 */
bool app_init(const struct config* cfg, const char* const* sources, size_t num);

/**
 * Destroy global application context.
 */
void app_destroy(void);

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
 * Switch mode (viewer/slideshow/gallery).
 * @param name name of the mode to set
 */
void app_switch_mode(const char* name);

/**
 * Handler of external event: reload image / reset state.
 */
void app_reload(void);

/**
 * Handler of external event: redraw window.
 */
void app_redraw(void);

/**
 * Notify the current mode about changes in the image list.
 * @param image updated image instance
 * @param event operation type
 */
void app_on_imglist(struct image* image, enum fsevent event);

/**
 * Handler of external event: window resized.
 */
void app_on_resize(void);

/**
 * Window rescaling handler.
 * @param scale new scale factor
 */
void app_on_scale(double scale);

/**
 * Handler of external event: mouse move.
 * @param mods key modifiers (ctrl/alt/shift)
 * @param btn mask with mouse buttons state
 * @param x,y window coordinates of mouse pointer
 * @param dx,dy delta between old and new position
 */
void app_on_mmove(uint8_t mods, uint32_t btn, size_t x, size_t y, ssize_t dx,
                  ssize_t dy);

/**
 * Handler of external event: mouse clock/scroll.
 * @param mods key modifiers (ctrl/alt/shift)
 * @param btn mask with mouse buttons state
 * @param x,y window coordinates of mouse pointer
 */
void app_on_mclick(uint8_t mods, uint32_t btn, size_t x, size_t y);

/**
 * Handler of external event: key/mouse press.
 * @param key code of key pressed
 * @param mods key modifiers (ctrl/alt/shift)
 */
void app_on_keyboard(xkb_keysym_t key, uint8_t mods);

/**
 * Add action to execution event queue.
 * @param action pointer to the action
 * @param fau flag to free action after execution
 */
void app_apply_action(const struct action* action, bool fau);
