// SPDX-License-Identifier: MIT
// Abstract UI interface.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "buildcfg.h"
#include "ui.h"

/** Abstract UI interface. */
struct ui {
    /**
     * Free UI context.
     * @param data pointer to the UI context
     */
    void (*free)(void* data);

    /**
     * Prepare the window system to read events.
     * @param data pointer to the UI context
     */
    void (*event_prep)(void* data);

    /**
     * Event handler complete notification.
     * @param data pointer to the UI context
     */
    void (*event_done)(void* data);

    /**
     * Begin window redraw procedure.
     * @param data pointer to the UI context
     * @return window pixmap
     */
    struct pixmap* (*draw_begin)(void* data);

    /**
     * Finalize window redraw procedure.
     * @param data pointer to the UI context
     */
    void (*draw_commit)(void* data);

    /**
     * Set window title.
     * @param data pointer to the UI context
     * @param title title to set
     */
    void (*set_title)(void* data, const char* title);

    /**
     * Set mouse pointer shape.
     * @param data pointer to the UI context
     * @param shape cursor shape to set
     */
    void (*set_cursor)(void* data, enum ui_cursor shape);

    /**
     * Set surface content type.
     * @param data pointer to the UI context
     * @param ctype content type to set
     */
    void (*set_ctype)(void* data, enum ui_ctype ctype);

    /**
     * Get window width.
     * @param data pointer to the UI context
     * @return window width in pixels
     */
    size_t (*get_width)(void* data);

    /**
     * Get window height.
     * @param data pointer to the UI context
     * @return window height in pixels
     */
    size_t (*get_height)(void* data);

    /**
     * Toggle full screen mode.
     * @param data pointer to the UI context
     */
    void (*toggle_fullscreen)(void* data);
};

#ifdef HAVE_WAYLAND
/**
 * Initialize Wayland UI.
 * @param app_id application id, used as window class
 * @param width,height initial window size in pixels
 * @param decor flag to use server-side window decoration
 * @param handlers Wayland-specific handlers
 * @return pointer to the UI context or NULL on errors
 */
void* ui_init_wl(const char* app_id, size_t width, size_t height, bool decor,
                 struct ui* handlers);
#endif // HAVE_WAYLAND

#ifdef HAVE_DRM
/**
 * Initialize DRM UI.
 * @param handlers DRM-specific handlers
 * @return pointer to the UI context or NULL on errors
 */
void* ui_init_drm(struct ui* handlers);
#endif // HAVE_DRM
