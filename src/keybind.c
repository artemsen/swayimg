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

// Section names in the config file
#define CONFIG_SECTION_KEYS  "keys"
#define CONFIG_SECTION_MOUSE "mouse"

// Default key bindings
struct keybind_default {
    xkb_keysym_t key;     ///< Keyboard key
    uint8_t mods;         ///< Key modifiers
    struct action action; ///< Action
};
// clang-format off
static const struct keybind_default default_bindings[] = {
    { .key = XKB_KEY_F1,          .action = { action_help, NULL } },
    { .key = XKB_KEY_Home,        .action = { action_first_file, NULL } },
    { .key = XKB_KEY_End,         .action = { action_last_file, NULL } },
    { .key = XKB_KEY_space,       .action = { action_next_file, NULL } },
    { .key = XKB_KEY_SunPageDown, .action = { action_next_file, NULL } },
    { .key = XKB_KEY_SunPageUp,   .action = { action_prev_file, NULL } },
    { .key = XKB_KEY_c,           .action = { action_skip_file, NULL } },
    { .key = XKB_KEY_d,           .action = { action_next_dir, NULL } },
    { .key = XKB_KEY_d, .mods = KEYMOD_SHIFT, .action = { action_prev_dir, NULL } },
    { .key = XKB_KEY_o,                       .action = { action_next_frame, NULL } },
    { .key = XKB_KEY_o, .mods = KEYMOD_SHIFT, .action = { action_prev_frame, NULL } },
    { .key = XKB_KEY_s,                       .action = { action_animation, NULL } },
    { .key = XKB_KEY_s, .mods = KEYMOD_SHIFT, .action = { action_slideshow, NULL } },
    { .key = XKB_KEY_f,           .action = { action_fullscreen, NULL } },
    { .key = XKB_KEY_Left,        .action = { action_step_left, NULL } },
    { .key = XKB_KEY_Right,       .action = { action_step_right, NULL } },
    { .key = XKB_KEY_Up,          .action = { action_step_up, NULL } },
    { .key = XKB_KEY_Down,        .action = { action_step_down, NULL } },
    { .key = XKB_KEY_equal,       .action = { action_zoom, "+10" } },
    { .key = XKB_KEY_plus,        .action = { action_zoom, "+10" } },
    { .key = XKB_KEY_minus,       .action = { action_zoom, "-10" } },
    { .key = XKB_KEY_w,           .action = { action_zoom, "width" } },
    { .key = XKB_KEY_w, .mods = KEYMOD_SHIFT, .action = { action_zoom, "height" } },
    { .key = XKB_KEY_z,                       .action = { action_zoom, "fit" } },
    { .key = XKB_KEY_z, .mods = KEYMOD_SHIFT, .action = { action_zoom, "fill" } },
    { .key = XKB_KEY_0,                       .action = { action_zoom, "real" } },
    { .key = XKB_KEY_BackSpace,               .action = { action_zoom, "optimal" } },
    { .key = XKB_KEY_bracketleft,             .action = { action_rotate_left, NULL } },
    { .key = XKB_KEY_bracketright,            .action = { action_rotate_right, NULL } },
    { .key = XKB_KEY_m,                       .action = { action_flip_vertical, NULL } },
    { .key = XKB_KEY_m, .mods = KEYMOD_SHIFT, .action = { action_flip_horizontal, NULL } },
    { .key = XKB_KEY_a,           .action = { action_antialiasing, NULL } },
    { .key = XKB_KEY_r,           .action = { action_reload, NULL } },
    { .key = XKB_KEY_i,           .action = { action_info, NULL } },
    { .key = XKB_KEY_e,           .action = { action_exec, "echo \"Image: %\"" } },
    { .key = XKB_KEY_Escape,      .action = { action_exit, NULL } },
    { .key = XKB_KEY_q,           .action = { action_exit, NULL } },
    { .key = VKEY_SCROLL_LEFT,    .action = { action_step_right, "5" } },
    { .key = VKEY_SCROLL_RIGHT,   .action = { action_step_left,  "5" } },
    { .key = VKEY_SCROLL_UP,      .action = { action_step_down,  "5" } },
    { .key = VKEY_SCROLL_DOWN,    .action = { action_step_up,    "5" } },
    { .key = VKEY_SCROLL_UP,   .mods = KEYMOD_CTRL,  .action = { action_zoom, "+10" } },
    { .key = VKEY_SCROLL_DOWN, .mods = KEYMOD_CTRL,  .action = { action_zoom, "-10" } },
    { .key = VKEY_SCROLL_UP,   .mods = KEYMOD_SHIFT, .action = { action_prev_file, NULL } },
    { .key = VKEY_SCROLL_DOWN, .mods = KEYMOD_SHIFT, .action = { action_next_file, NULL } },
    { .key = VKEY_SCROLL_UP,   .mods = KEYMOD_ALT,   .action = { action_prev_frame, NULL } },
    { .key = VKEY_SCROLL_DOWN, .mods = KEYMOD_ALT,   .action = { action_next_frame, NULL } },
};
// clang-format on

// Names of virtual keys
struct virtual_keys {
    xkb_keysym_t key;
    const char* name;
};
static const struct virtual_keys virtual_keys[] = {
    { VKEY_SCROLL_UP, "ScrollUp" },
    { VKEY_SCROLL_DOWN, "ScrollDown" },
    { VKEY_SCROLL_LEFT, "ScrollLeft" },
    { VKEY_SCROLL_RIGHT, "ScrollRight" },
};

struct key_binding* key_bindings;
size_t key_bindings_size;

/**
 * Remove all actions from binding.
 * @param binding key binding
 */
static void free_binding(struct key_binding* binding)
{
    struct action* it = binding->actions;
    while (it && it->type != action_none) {
        action_free(it);
        ++it;
    }
    free(binding->actions);
    free(binding->help);
    memset(binding, 0, sizeof(*binding));
}

/**
 * Create new key binding.
 * @param key keyboard key
 * @param mods key modifiers (ctrl/alt/shift)
 * @return pointer to the binding description
 */
static struct key_binding* create_binding(xkb_keysym_t key, uint8_t mods)
{
    struct key_binding* binding = NULL;

    // search for empty node
    for (size_t i = 0; i < key_bindings_size; ++i) {
        struct key_binding* kb = &key_bindings[i];
        if (kb->key == key && kb->mods == mods) {
            binding = kb; // replace existing
            break;
        } else if (!kb->actions && !binding) {
            binding = kb; // reuse empty node
        }
    }

    if (binding) {
        free_binding(binding);
    } else {
        // add new node (reallocate)
        const size_t sz = (key_bindings_size + 1) * sizeof(struct key_binding);
        struct key_binding* kb = realloc(key_bindings, sz);
        if (!kb) {
            return NULL;
        }
        binding = &kb[key_bindings_size];
        memset(binding, 0, sizeof(*binding));
        key_bindings = kb;
        ++key_bindings_size;
    }

    binding->key = key;
    binding->mods = mods;

    return binding;
}

/**
 * Add one more action for specified key binding.
 * @param binding target binding
 * @param action action to add
 * @return true if action added
 */
static bool add_action(struct key_binding* binding, const struct action* action)
{
    struct action* buf;
    size_t size = 1;

    if (binding->actions) {
        while (binding->actions[size].type != action_none) {
            ++size;
        }
        ++size;
    }

    buf = realloc(binding->actions, (size + 1) * sizeof(struct action));
    if (buf) {
        binding->actions = buf;
        action_dup(action, &binding->actions[size - 1]);
        binding->actions[size].type = action_none;
        binding->actions[size].params = NULL;
    }

    // construct help description
    if (!binding->help) {
        char* key_name = keybind_name(binding->key, binding->mods);
        if (key_name) {
            str_append(key_name, 0, &binding->help);
            str_append(": ", 2, &binding->help);
            str_append(action_typename(action), 0, &binding->help);
            if (action->params) {
                str_append(" ", 1, &binding->help);
                str_append(action->params, 0, &binding->help);
            }
            free(key_name);
        }
    }

    return !!buf;
}

/**
 * Get a keysym from its name.
 * @param key keyboard key
 * @param mods key modifiers (ctrl/alt/shift)
 * @return false if name is invalid
 */
static bool parse_key(const char* name, xkb_keysym_t* key, uint8_t* mods)
{
    struct str_slice slices[4]; // mod[alt+ctrl+shift]+key
    const size_t snum = str_split(name, '+', slices, ARRAY_SIZE(slices));
    if (snum == 0) {
        return false;
    }

    // get modifiers
    *mods = 0;
    for (size_t i = 0; i < snum - 1; ++i) {
        const char* mod_names[] = { "Ctrl", "Alt", "Shift" };
        const ssize_t index =
            str_index(mod_names, slices[i].value, slices[i].len);
        if (index < 0) {
            return false;
        }
        *mods |= 1 << index;
    }

    // get key
    *key = xkb_keysym_from_name(slices[snum - 1].value,
                                XKB_KEYSYM_CASE_INSENSITIVE);
    // check for virtual keys
    if (*key == XKB_KEY_NoSymbol) {
        for (size_t i = 0; i < ARRAY_SIZE(virtual_keys); ++i) {
            if (strcmp(slices[snum - 1].value, virtual_keys[i].name) == 0) {
                *key = virtual_keys[i].key;
                break;
            }
        }
    }
    // check for international symbols
    if (*key == XKB_KEY_NoSymbol) {
        wchar_t* wide = str_to_wide(slices[snum - 1].value, NULL);
        *key = xkb_utf32_to_keysym(wide[0]);
        free(wide);
    }

    return (*key != XKB_KEY_NoSymbol);
}

/**
 * Custom section loader, see `config_loader` for details.
 */
static enum config_status load_config(const char* key, const char* value)
{
    struct key_binding* binding;
    xkb_keysym_t keysym;
    uint8_t mods;

    struct str_slice slices[32];
    const size_t num = str_split(value, ';', slices, ARRAY_SIZE(slices));

    if (num == 0) {
        return cfgst_invalid_value;
    }

    // convert key name to code
    if (!parse_key(key, &keysym, &mods)) {
        return cfgst_invalid_key;
    }

    binding = create_binding(keysym, mods);
    if (!binding) {
        return cfgst_invalid_key;
    }

    for (size_t i = 0; i < num; ++i) {
        struct action action;
        if (slices[i].len == 0) {
            continue;
        }
        if (!action_load(&action, slices[i].value, slices[i].len)) {
            return cfgst_invalid_value;
        }
        if (action.type == action_none) {
            continue;
        }
        if (!add_action(binding, &action)) {
            action_free(&action);
            return cfgst_invalid_value;
        }
        action_free(&action);
    }

    return cfgst_ok;
}

void keybind_init(void)
{
    // set defaults
    for (size_t i = 0; i < ARRAY_SIZE(default_bindings); ++i) {
        const struct keybind_default* kb = &default_bindings[i];
        struct key_binding* binding = create_binding(kb->key, kb->mods);
        if (binding) {
            add_action(binding, &kb->action);
        }
    }

    // register configuration loader
    config_add_loader(CONFIG_SECTION_KEYS, load_config);
    config_add_loader(CONFIG_SECTION_MOUSE, load_config);
}

void keybind_free(void)
{
    for (size_t i = 0; i < key_bindings_size; ++i) {
        free_binding(&key_bindings[i]);
    }
    free(key_bindings);
}

uint8_t keybind_mods(struct xkb_state* state)
{
    uint8_t mods = 0;

    if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_CTRL,
                                     XKB_STATE_MODS_EFFECTIVE) > 0) {
        mods |= KEYMOD_CTRL;
    }
    if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_ALT,
                                     XKB_STATE_MODS_EFFECTIVE) > 0) {
        mods |= KEYMOD_ALT;
    }
    if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_SHIFT,
                                     XKB_STATE_MODS_EFFECTIVE) > 0) {
        mods |= KEYMOD_SHIFT;
    }

    return mods;
}

char* keybind_name(xkb_keysym_t key, uint8_t mods)
{
    char key_name[32];
    char* name = NULL;

    // skip modifiers
    switch (key) {
        case XKB_KEY_Shift_L:
        case XKB_KEY_Shift_R:
        case XKB_KEY_Control_L:
        case XKB_KEY_Control_R:
        case XKB_KEY_Meta_L:
        case XKB_KEY_Meta_R:
        case XKB_KEY_Alt_L:
        case XKB_KEY_Alt_R:
            return NULL;
    }

    if (xkb_keysym_get_name(key, key_name, sizeof(key_name)) > 0) {
        if (mods & KEYMOD_CTRL) {
            str_append("Ctrl+", 0, &name);
        }
        if (mods & KEYMOD_ALT) {
            str_append("Alt+", 0, &name);
        }
        if (mods & KEYMOD_SHIFT) {
            str_append("Shift+", 0, &name);
        }
        str_append(key_name, 0, &name);
    }

    return name;
}

const struct action* keybind_actions(xkb_keysym_t key, uint8_t mods)
{
    // we always use lowercase + Shift modifier
    key = xkb_keysym_to_lower(key);

    for (size_t i = 0; i < key_bindings_size; ++i) {
        struct key_binding* binding = &key_bindings[i];
        if (binding->key == key && binding->mods == mods) {
            return binding->actions;
        }
    }

    return NULL;
}
