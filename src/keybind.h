// SPDX-License-Identifier: MIT
// Keyboard bindings.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <xkbcommon/xkbcommon.h>

/** Available actions. */
enum kb_action {
    kb_none,
    kb_first_file,
    kb_last_file,
    kb_prev_dir,
    kb_next_dir,
    kb_prev_file,
    kb_next_file,
    kb_prev_frame,
    kb_next_frame,
    kb_animation,
    kb_slideshow,
    kb_fullscreen,
    kb_step_left,
    kb_step_right,
    kb_step_up,
    kb_step_down,
    kb_zoom_in,
    kb_zoom_out,
    kb_zoom_optimal,
    kb_zoom_fit_width,
    kb_zoom_fit_height,
    kb_zoom_fit,   // Fit largest dimension
    kb_zoom_fill,  // Fit smallest dimension
    kb_zoom_real,
    kb_zoom_reset,
    kb_rotate_left,
    kb_rotate_right,
    kb_flip_vertical,
    kb_flip_horizontal,
    kb_reload,
    kb_antialiasing,
    kb_info,
    kb_exec,
    kb_quit,
};

/** Key binding. */
struct key_binding {
    xkb_keysym_t key;      ///< Keyboard key
    enum kb_action action; ///< Action
    char* params;          ///< Custom parameters for the action
};

/**
 * Initialize default key bindings.
 */
void keybind_init(void);

/**
 * Free key binding context.
 */
void keybind_free(void);

/**
 * Get key binding description.
 * @param key keyboard key
 * @return pointer to the binding or NULL if not found
 */
const struct key_binding* keybind_get(xkb_keysym_t key);
