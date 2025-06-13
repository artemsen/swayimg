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
    action_rand_file,
    action_skip_file,
    action_prev_frame,
    action_next_frame,
    action_animation,
    action_slideshow,
    action_fullscreen,
    action_mode,
    action_step_left,
    action_step_right,
    action_step_up,
    action_step_down,
    action_page_up,
    action_page_down,
    action_zoom,
    action_thumb,
    action_rotate_left,
    action_rotate_right,
    action_flip_vertical,
    action_flip_horizontal,
    action_reload,
    action_antialiasing,
    action_info,
    action_exec,
    action_export,
    action_status,
    action_exit,
    // internal usage only
    action_redraw,
    action_drag,
};

/** Single action. */
struct action {
    struct action* next;   ///< Pinter to the next action in the sequence
    enum action_type type; ///< Action type
    char params[1];        ///< Custom parameters for the action (variable len)
};

/**
 * Create action sequence from config string.
 * @param text source config text
 * @param actions destination sequence of actions
 * @return parsed sequence of actions, NULL if format error
 */
struct action* action_create(const char* text);

/**
 * Free actions sequence.
 * @param actions sequence to free
 */
void action_free(struct action* actions);

/**
 * Get action's type name.
 * @param action to get type
 * @return type name
 */
const char* action_typename(const struct action* action);
