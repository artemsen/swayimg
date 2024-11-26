// SPDX-License-Identifier: MIT
// Events processed by the viewer and gallery.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "action.h"
#include "image.h"

/** Event types. */
enum event_type {
    event_action,   ///< Apply action
    event_redraw,   ///< Redraw window request
    event_resize,   ///< Window resize notification
    event_drag,     ///< Mouse or touch drag operation
    event_load,     ///< Image loaded (preload thread notification)
    event_activate, ///< The mode is activating (viewer/gallery switch)
};

/** Event description. */
struct event {
    enum event_type type;

    union event_params {

        const struct action* action;

        struct drag {
            int dx;
            int dy;
        } drag;

        struct activate {
            size_t index;
        } activate;

        struct load {
            struct image* image;
            size_t index;
        } load;

    } param;
};

/**
 * Event handler declaration.
 * @param event event to handle
 */
typedef void (*event_handler)(const struct event* event);

/**
 * Create notification (eventfd descriptor).
 * @return file descriptor or -1 on errors
 */
int notification_create(void);

/**
 * Free notification instance.
 * @param fd file descriptor for the notification
 */
void notification_free(int fd);

/**
 * Send notification through file descriptor.
 * @param fd file descriptor for the notification
 */
void notification_raise(int fd);

/**
 * Reset notification after raising.
 * @param fd file descriptor for the notification
 */
void notification_reset(int fd);
