// SPDX-License-Identifier: MIT
// Actions: set of predefined commands to execute.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "action.h"

#include "str.h"

#include <ctype.h>
#include <stdlib.h>

/** Action names. */
static const char* action_names[] = {
    [action_none] = "none",
    [action_help] = "help",
    [action_first_file] = "first_file",
    [action_last_file] = "last_file",
    [action_prev_dir] = "prev_dir",
    [action_next_dir] = "next_dir",
    [action_prev_file] = "prev_file",
    [action_next_file] = "next_file",
    [action_skip_file] = "skip_file",
    [action_prev_frame] = "prev_frame",
    [action_next_frame] = "next_frame",
    [action_animation] = "animation",
    [action_slideshow] = "slideshow",
    [action_fullscreen] = "fullscreen",
    [action_step_left] = "step_left",
    [action_step_right] = "step_right",
    [action_step_up] = "step_up",
    [action_step_down] = "step_down",
    [action_zoom] = "zoom",
    [action_rotate_left] = "rotate_left",
    [action_rotate_right] = "rotate_right",
    [action_flip_vertical] = "flip_vertical",
    [action_flip_horizontal] = "flip_horizontal",
    [action_reload] = "reload",
    [action_antialiasing] = "antialiasing",
    [action_info] = "info",
    [action_exec] = "exec",
    [action_status] = "status",
    [action_exit] = "exit",
};

bool action_load(struct action* action, const char* source, size_t len)
{
    ssize_t action_type;
    const char* action_name;
    size_t action_len;
    const char* params;
    size_t params_len;
    size_t pos = 0;

    // skip spaces
    while (pos < len && isspace(source[pos])) {
        ++pos;
    }

    action_name = &source[pos];

    // get action type
    action_len = pos;
    while (pos < len && !isspace(source[pos])) {
        ++pos;
    }
    action_len = pos - action_len;
    action_type = str_index(action_names, action_name, action_len);
    if (action_type < 0) {
        return false;
    }
    action->type = action_type;

    // skip spaces
    while (pos < len && isspace(source[pos])) {
        ++pos;
    }

    // rest part: parameters
    params = &source[pos];
    params_len = len - pos;
    if (params_len) {
        action->params = str_append(params, params_len, NULL);
        if (!action->params) {
            return false;
        }
    } else {
        action->params = NULL;
    }

    return true;
}

const char* action_typename(const struct action* action)
{
    if (action->type < ARRAY_SIZE(action_names)) {
        return action_names[action->type];
    }
    return NULL;
}
