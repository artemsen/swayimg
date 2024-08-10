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
    [action_mode] = "mode",
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

/**
 * Load action from config string.
 * @param action target instance, caller should free resources
 * @param source action command with parameters as text string
 * @param len length of the source string
 * @return true if action loaded
 */
static bool action_load(struct action* action, const char* source, size_t len)
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

size_t action_create(const char* text, struct action_seq* actions)
{
    struct str_slice slices[ACTION_SEQ_MAX];
    size_t num;

    num = str_split(text, ';', slices, ARRAY_SIZE(slices));
    if (num == 0) {
        return 0;
    }
    if (num > actions->num) {
        num = actions->num;
    }

    for (size_t i = 0; i < num; ++i) {
        struct action* action = &actions->sequence[i];
        struct str_slice* s = &slices[i];
        if (!action_load(action, s->value, s->len)) {
            const size_t origin_num = actions->num;
            actions->num = i;
            action_free(actions);
            actions->num = origin_num;
            return 0;
        }
    }

    actions->num = num;
    return num;
}

void action_free(struct action_seq* actions)
{
    if (actions) {
        for (size_t i = 0; i < actions->num; ++i) {
            free(actions->sequence[i].params);
        }
    }
}

const char* action_typename(const struct action* action)
{
    if (action->type < ARRAY_SIZE(action_names)) {
        return action_names[action->type];
    }
    return NULL;
}
