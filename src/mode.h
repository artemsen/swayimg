// SPDX-License-Identifier: MIT
// Mode handlers.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "fs.h"
#include "image.h"
#include "keybind.h"

/** Mode types. */
enum mode_type {
    mode_viewer,
    mode_gallery,
};

/**
 * Set of event handlers used to communicate with viewer and gallery.
 */
struct mode {

    /**
     * Activate mode (viewer/gallery switch).
     * @param image image to show/select
     */
    void (*on_activate)(struct image* image);

    /**
     * Deactivate mode (viewer/gallery switch).
     */
    void (*on_deactivate)(void);

    /**
     * Window resize handler.
     */
    void (*on_resize)(void);

    /**
     * Mouse move handler.
     * @param mods key modifiers (ctrl/alt/shift)
     * @param btn mask with mouse buttons state
     * @param x,y window coordinates of mouse pointer
     * @param dx,dy delta between old and new position since last call
     */
    void (*on_mouse_move)(uint8_t mods, uint32_t btn, size_t x, size_t y,
                          ssize_t dx, ssize_t dy);

    /**
     * Mouse click/scroll handler.
     * @param mods key modifiers (ctrl/alt/shift)
     * @param btn mask with mouse buttons state
     * @param x,y window coordinates of mouse pointer
     * @return true if click was handled
     */
    bool (*on_mouse_click)(uint8_t mods, uint32_t btn, size_t x, size_t y);

    /**
     * Image list update handler.
     * @param image updated image instance
     * @param event operation type
     */
    void (*on_imglist)(const struct image* image, enum fsevent event);

    /**
     * Handle action.
     * @param action action to apply
     */
    void (*handle_action)(const struct action* action);

    /**
     * Get currently showed/selected image.
     * @return current image
     */
    struct image* (*get_current)(void);

    /**
     * Get key bindings.
     * @return head of key binding list
     */
    struct keybind* (*get_keybinds)(void);
};

/**
 * Handle action.
 * @param mode mode handlers
 * @param action action to apply
 */
void mode_action(struct mode* mode, const struct action* action);
