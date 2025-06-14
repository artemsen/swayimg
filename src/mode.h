// SPDX-License-Identifier: MIT
// Mode handlers.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "action.h"
#include "fs.h"
#include "image.h"

/** Mode types. */
enum mode_type {
    mode_viewer,
    mode_gallery,
};

/**
 * Set of event handlers used to communicate with viewer and gallery.
 */
struct mode_handlers {
    /**
     * Apply action.
     * @param action action to apply
     */
    void (*action)(const struct action* action);

    /**
     * Redraw window.
     * @param window target window
     */
    void (*redraw)(struct pixmap* window);

    /**
     * Window resize handler.
     */
    void (*resize)(void);

    /**
     * Mouse move handler.
     * @param mods key modifiers (ctrl/alt/shift)
     * @param btn mask with mouse buttons state
     * @param x,y window coordinates of mouse pointer
     * @param dx,dy delta between old and new position since last call
     */
    void (*mouse_move)(uint8_t mods, uint32_t btn, size_t x, size_t y,
                       ssize_t dx, ssize_t dy);

    /**
     * Mouse click/scroll handler.
     * @param mods key modifiers (ctrl/alt/shift)
     * @param btn mask with mouse buttons state
     * @param x,y window coordinates of mouse pointer
     * @return true if click was handled
     */
    bool (*mouse_click)(uint8_t mods, uint32_t btn, size_t x, size_t y);

    /**
     * Image list update handler.
     * @param image updated image instance
     * @param event operation type
     */
    void (*imglist)(const struct image* image, enum fsevent event);

    /**
     * Get currently showed/selected image.
     * @return current image
     */
    struct image* (*current)(void);

    /**
     * Activate mode (viewer/gallery switch).
     * @param image image to show/select
     */
    void (*activate)(struct image* image);

    /**
     * Deactivate mode (viewer/gallery switch).
     * @return currently showed/selected image
     */
    struct image* (*deactivate)(void);
};
