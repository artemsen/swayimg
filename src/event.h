// SPDX-License-Identifier: MIT
// Application events.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "keybind.h"
#include "pixmap.h"

/** Event types. */
enum event_type {
    event_reload,
    event_redraw,
    event_resize,
    event_keypress,
    event_drag,
};

/** Event description. */
struct event {
    enum event_type type;

    union event_params {

        struct keypress {
            xkb_keysym_t key;
            uint8_t mods;
        } keypress;

        struct drag {
            int dx;
            int dy;
        } drag;

        struct exit {
            int rc;
        } exit;

    } param;
};

/**
 * Event handler declaration.
 * @param event event to handle
 */
typedef void (*event_handler)(const struct event* event);
