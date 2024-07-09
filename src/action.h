// SPDX-License-Identifier: MIT
// Actions: set of predefined commands to execute.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stdbool.h>
#include <stddef.h>

/** Supported actions. */
enum action_type {
    action_none,
    action_help,
    action_first_file,
    action_last_file,
    action_prev_dir,
    action_next_dir,
    action_prev_file,
    action_next_file,
    action_skip_file,
    action_prev_frame,
    action_next_frame,
    action_animation,
    action_slideshow,
    action_fullscreen,
    action_step_left,
    action_step_right,
    action_step_up,
    action_step_down,
    action_zoom,
    action_rotate_left,
    action_rotate_right,
    action_flip_vertical,
    action_flip_horizontal,
    action_reload,
    action_antialiasing,
    action_info,
    action_exec,
    action_status,
    action_exit,
};

/** Action instance. */
struct action {
    enum action_type type; ///< Action type
    char* params;          ///< Custom parameters for the action
};

/**
 * Load action from config string.
 * @param action target instance, caller should free resources
 * @param source action command with parameters as text string
 * @param len length of the source string
 * @return true if action loaded
 */
bool action_load(struct action* action, const char* source, size_t len);

/**
 * Get action's type name.
 * @param action to get type
 * @return type name
 */
const char* action_typename(const struct action* action);
