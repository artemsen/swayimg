// SPDX-License-Identifier: MIT
// Actions: set of predefined commands to execute.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

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
    action_mode,
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

// Max number of actions in sequence
#define ACTION_SEQ_MAX 32

/** Single action. */
struct action {
    enum action_type type; ///< Action type
    char* params;          ///< Custom parameters for the action
};

/** Action sequence. */
struct action_seq {
    struct action* sequence; ///< Array of actions
    size_t num;              ///< Number of actions in array
};

/**
 * Create sequence of actions from config string.
 * @param text source config text
 * @param actions destination sequence of actions
 * @return num number of actions in the list, 0 if format error
 */
size_t action_create(const char* text, struct action_seq* actions);

/**
 * Free actions.
 * @param actions sequence of actions to free
 */
void action_free(struct action_seq* actions);

/**
 * Get action's type name.
 * @param action to get type
 * @return type name
 */
const char* action_typename(const struct action* action);
