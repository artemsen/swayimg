// SPDX-License-Identifier: MIT
// Actions: set of predefined commands to execute.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "action.h"

#include "array.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// Max number of actions in sequence
#define ACTION_SEQ_MAX 32

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
    [action_rand_file] = "rand_file",
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
    [action_page_up] = "page_up",
    [action_page_down] = "page_down",
    [action_zoom] = "zoom",
    [action_thumb] = "thumb",
    [action_rotate_left] = "rotate_left",
    [action_rotate_right] = "rotate_right",
    [action_flip_vertical] = "flip_vertical",
    [action_flip_horizontal] = "flip_horizontal",
    [action_reload] = "reload",
    [action_redraw] = "redraw",
    [action_drag] = "drag",
    [action_antialiasing] = "antialiasing",
    [action_info] = "info",
    [action_exec] = "exec",
    [action_export] = "export",
    [action_status] = "status",
    [action_exit] = "exit",
};

/**
 * Parse config line and fill the action.
 * @param source action command with parameters as text string
 * @param len length of the source string
 * @return parsed action, NULL if format error
 */
static struct action* parse(const char* source, size_t len)
{
    struct action* action;
    ssize_t action_type;
    const char* action_name;

    // trim spaces
    while (len && isspace(*source)) {
        --len;
        ++source;
    }
    while (len && isspace(source[len - 1])) {
        --len;
    }

    // get action type
    action_name = source;
    while (len && !isspace(*source)) {
        --len;
        ++source;
    }
    action_type = str_index(action_names, action_name, source - action_name);
    if (action_type < 0) {
        return NULL;
    }

    // skip spaces
    while (len && isspace(*source)) {
        --len;
        ++source;
    }

    // create action
    action = malloc(sizeof(struct action) + len);
    if (action) {
        action->next = NULL;
        action->type = action_type;
        if (len) {
            memcpy(action->params, source, len);
        }
        action->params[len] = 0;
    }
    return action;
}

struct action* action_create(const char* text)
{
    struct action* actions = NULL;
    struct str_slice slices[ACTION_SEQ_MAX];
    size_t seq_len;

    // split line, one slice per action
    seq_len = str_split(text, ';', slices, ARRAY_SIZE(slices));
    if (seq_len == 0) {
        return NULL;
    }
    if (seq_len > ARRAY_SIZE(slices)) {
        seq_len = ARRAY_SIZE(slices);
    }

    // load sequence of actions
    for (size_t i = 0; i < seq_len; ++i) {
        const struct str_slice* s = &slices[i];
        struct action* action = parse(s->value, s->len);
        if (action) {
            if (actions) {
                struct action* last = actions;
                while (last->next) {
                    last = last->next;
                }
                last->next = action;
            } else {
                actions = action;
            }
        } else {
            action_free(actions);
            actions = NULL;
            break;
        }
    }

    return actions;
}

void action_free(struct action* actions)
{
    while (actions) {
        struct action* entry = actions;
        actions = actions->next;
        free(entry);
    }
}

const char* action_typename(const struct action* action)
{
    if (action->type < ARRAY_SIZE(action_names)) {
        return action_names[action->type];
    }
    return NULL;
}
