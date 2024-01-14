// SPDX-License-Identifier: MIT
// Keyboard bindings.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#include "keybind.h"

#include "config.h"
#include "str.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// Section name in the config file
#define CONFIG_SECTION "keys"

/** Action names. */
static const char* action_names[] = {
    [kb_none] = "none",
    [kb_help] = "help",
    [kb_first_file] = "first_file",
    [kb_last_file] = "last_file",
    [kb_prev_dir] = "prev_dir",
    [kb_next_dir] = "next_dir",
    [kb_prev_file] = "prev_file",
    [kb_next_file] = "next_file",
    [kb_prev_frame] = "prev_frame",
    [kb_next_frame] = "next_frame",
    [kb_animation] = "animation",
    [kb_slideshow] = "slideshow",
    [kb_fullscreen] = "fullscreen",
    [kb_step_left] = "step_left",
    [kb_step_right] = "step_right",
    [kb_step_up] = "step_up",
    [kb_step_down] = "step_down",
    [kb_zoom] = "zoom",
    [kb_rotate_left] = "rotate_left",
    [kb_rotate_right] = "rotate_right",
    [kb_flip_vertical] = "flip_vertical",
    [kb_flip_horizontal] = "flip_horizontal",
    [kb_reload] = "reload",
    [kb_antialiasing] = "antialiasing",
    [kb_info] = "info",
    [kb_exec] = "exec",
    [kb_exit] = "exit",
};

// Default key bindings
static const struct key_binding default_bindings[] = {
    { .key = XKB_KEY_F1, .action = kb_help },
    { .key = XKB_KEY_Home, .action = kb_first_file },
    { .key = XKB_KEY_End, .action = kb_last_file },
    { .key = XKB_KEY_space, .action = kb_next_file },
    { .key = XKB_KEY_SunPageDown, .action = kb_next_file },
    { .key = XKB_KEY_SunPageUp, .action = kb_prev_file },
    { .key = XKB_KEY_d, .action = kb_next_dir },
    { .key = XKB_KEY_D, .action = kb_prev_dir },
    { .key = XKB_KEY_o, .action = kb_next_frame },
    { .key = XKB_KEY_O, .action = kb_prev_frame },
    { .key = XKB_KEY_s, .action = kb_animation },
    { .key = XKB_KEY_S, .action = kb_slideshow },
    { .key = XKB_KEY_f, .action = kb_fullscreen },
    { .key = XKB_KEY_Left, .action = kb_step_left },
    { .key = XKB_KEY_Right, .action = kb_step_right },
    { .key = XKB_KEY_Up, .action = kb_step_up },
    { .key = XKB_KEY_Down, .action = kb_step_down },
    { .key = XKB_KEY_KP_Left, .action = kb_step_left },
    { .key = XKB_KEY_KP_Right, .action = kb_step_right },
    { .key = XKB_KEY_KP_Up, .action = kb_step_up },
    { .key = XKB_KEY_KP_Down, .action = kb_step_down },
    { .key = XKB_KEY_equal, .action = kb_zoom, .params = "+10" },
    { .key = XKB_KEY_plus, .action = kb_zoom, .params = "+10" },
    { .key = XKB_KEY_minus, .action = kb_zoom, .params = "-10" },
    { .key = XKB_KEY_w, .action = kb_zoom, .params = "width" },
    { .key = XKB_KEY_W, .action = kb_zoom, .params = "height" },
    { .key = XKB_KEY_z, .action = kb_zoom, .params = "fit" },
    { .key = XKB_KEY_Z, .action = kb_zoom, .params = "fill" },
    { .key = XKB_KEY_0, .action = kb_zoom, .params = "real" },
    { .key = XKB_KEY_BackSpace, .action = kb_zoom, .params = "optimal" },
    { .key = XKB_KEY_bracketleft, .action = kb_rotate_left },
    { .key = XKB_KEY_bracketright, .action = kb_rotate_right },
    { .key = XKB_KEY_m, .action = kb_flip_vertical },
    { .key = XKB_KEY_M, .action = kb_flip_horizontal },
    { .key = XKB_KEY_a, .action = kb_antialiasing },
    { .key = XKB_KEY_r, .action = kb_reload },
    { .key = XKB_KEY_i, .action = kb_info },
    { .key = XKB_KEY_e, .action = kb_exec, .params = "echo \"Image: %\"" },
    { .key = XKB_KEY_Escape, .action = kb_exit },
    { .key = XKB_KEY_q, .action = kb_exit },
};

struct key_binding* key_bindings;
size_t key_bindings_size;

/**
 * Set key binding.
 * @param key keyboard key
 * @param action action to set
 * @param params additional parameters (action specific)
 */
static void keybind_set(xkb_keysym_t key, enum kb_action action,
                        const char* params)
{
    char key_name[32];
    char* help = NULL;
    struct key_binding* new_binding = NULL;

    for (size_t i = 0; i < key_bindings_size; ++i) {
        struct key_binding* binding = &key_bindings[i];
        if (binding->key == key) {
            new_binding = binding; // replace existing
            break;
        } else if (binding->action == kb_none && !new_binding) {
            new_binding = binding; // reuse existing
        }
    }

    if (action == kb_none) {
        // remove existing binding
        if (new_binding) {
            new_binding->action = action;
            free(new_binding->params);
            new_binding->params = NULL;
            free(new_binding->help);
            new_binding->help = NULL;
        }
        return;
    }

    if (!new_binding) {
        // add new (reallocate)
        const size_t sz = (key_bindings_size + 1) * sizeof(struct key_binding);
        struct key_binding* bindings = realloc(key_bindings, sz);
        if (!bindings) {
            return;
        }
        new_binding = &bindings[key_bindings_size];
        memset(new_binding, 0, sizeof(*new_binding));
        key_bindings = bindings;
        ++key_bindings_size;
    }

    // set new parameters
    new_binding->key = key;
    new_binding->action = action;
    if (new_binding->params && (!params || !*params)) {
        free(new_binding->params);
        new_binding->params = NULL;
    } else if (params && *params) {
        str_dup(params, &new_binding->params);
    }

    // construct help description
    xkb_keysym_get_name(key, key_name, sizeof(key_name));
    str_append(key_name, 0, &help);
    str_append(" ", 1, &help);
    str_append(action_names[action], 0, &help);
    if (new_binding->params) {
        str_append(" ", 1, &help);
        str_append(new_binding->params, 0, &help);
    }
    str_to_wide(help, &new_binding->help);
    free(help);
}

/**
 * Custom section loader, see `config_loader` for details.
 */
static enum config_status load_config(const char* key, const char* value)
{
    enum kb_action action_id = kb_none;
    size_t action_len;
    const char* params;
    xkb_keysym_t keysym;
    const char* action_end;
    ssize_t index;

    // get action length
    action_end = value;
    while (*action_end && !isspace(*action_end)) {
        ++action_end;
    }
    action_len = action_end - value;

    // get action id form its name
    index = str_index(action_names, value, action_len);
    if (index < 0) {
        return cfgst_invalid_value;
    }
    action_id = index;

    // get parameters
    params = action_end;
    while (*params && isspace(*params)) {
        ++params;
    }
    if (!*params) {
        params = NULL;
    }

    // convert key name to code
    keysym = xkb_keysym_from_name(key, XKB_KEYSYM_NO_FLAGS);
    if (keysym == XKB_KEY_NoSymbol) {
        return cfgst_invalid_key;
    }

    keybind_set(keysym, action_id, params);

    return cfgst_ok;
}

void keybind_init(void)
{
    // set defaults
    for (size_t i = 0;
         i < sizeof(default_bindings) / sizeof(default_bindings[0]); ++i) {
        const struct key_binding* kb = &default_bindings[i];
        keybind_set(kb->key, kb->action, kb->params);
    }

    // register configuration loader
    config_add_loader(CONFIG_SECTION, load_config);
}

void keybind_free(void)
{
    for (size_t i = 0; i < key_bindings_size; ++i) {
        free(key_bindings[i].params);
        free(key_bindings[i].help);
    }
    free(key_bindings);
}

const struct key_binding* keybind_get(xkb_keysym_t key)
{
    for (size_t i = 0; i < key_bindings_size; ++i) {
        struct key_binding* binding = &key_bindings[i];
        if (binding->key == key) {
            return binding;
        }
    }
    return NULL;
}
