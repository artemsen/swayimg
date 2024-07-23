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
    { .key = XKB_KEY_Return,      .action = { action_mode, NULL } },
    { .key = XKB_KEY_Escape,      .action = { action_exit, NULL } },
    { .key = XKB_KEY_q,           .action = { action_exit, NULL } },
    { .key = VKEY_SCROLL_LEFT,    .action = { action_step_right, "5" } },
    { .key = VKEY_SCROLL_RIGHT,   .action = { action_step_left,  "5" } },
    { .key = VKEY_SCROLL_UP,      .action = { action_step_up,    "5" } },
    { .key = VKEY_SCROLL_DOWN,    .action = { action_step_down,  "5" } },
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

/** Head of global key binding list. */
static struct keybind* binding_list;

/**
 * Convert text name to key code.
 * @param name key text name
 * @param key output keyboard key
 * @param mods output key modifiers (ctrl/alt/shift)
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
 * Allocate new key binding.
 * @param key keyboard key
 * @param mods key modifiers (ctrl/alt/shift)
 * @param action list of actions to add
 * @param sz number of actions in list
 */
static struct keybind* keybind_alloc(xkb_keysym_t key, uint8_t mods,
                                     const struct action* action, size_t sz)
{
    struct keybind* kb;

    if (!action || sz == 0) {
        return NULL;
    }

    kb = calloc(1, sizeof(struct keybind) + sizeof(struct action) * sz);
    if (!kb) {
        return NULL;
    }

    kb->key = key;
    kb->mods = mods;

    // construct actions
    kb->actions = (struct action*)((uint8_t*)kb + sizeof(struct keybind));
    kb->num_actions = sz;
    for (size_t i = 0; i < sz; ++i) {
        kb->actions[i].type = action[i].type;
        if (action[i].params) {
            kb->actions[i].params = str_dup(action[i].params, NULL);
        }
    }

    // construct help description
    if (sz != 1 || kb->actions[0].type != action_none) {
        char* key_name = keybind_name(key, mods);
        if (key_name) {
            const struct action* action = &kb->actions[0]; // first only
            str_append(key_name, 0, &kb->help);
            str_append(": ", 2, &kb->help);
            str_append(action_typename(action), 0, &kb->help);
            if (action->params) {
                str_append(" ", 1, &kb->help);
                str_append(action->params, 0, &kb->help);
            }
            free(key_name);
        }
    }

    return kb;
}

/**
 * Free key binding.
 * @param kb key binding
 */
static void keybind_free(struct keybind* kb)
{
    if (kb) {
        for (size_t i = 0; i < kb->num_actions; ++i) {
            free(kb->actions[i].params);
        }
        free(kb->help);
        free(kb);
    }
}

/**
 * Put key binding to the global scheme.
 * @param key keyboard key
 * @param mods key modifiers (ctrl/alt/shift)
 * @param action list of actions to add
 * @param sz number of actions in list
 */
static void keybind_put(xkb_keysym_t key, uint8_t mods,
                        const struct action* action, size_t sz)
{
    struct keybind* kb;
    struct keybind* prev;

    // remove existing binding
    kb = binding_list;
    prev = NULL;
    while (kb) {
        if (kb->key == key && kb->mods == mods) {
            if (prev) {
                prev->next = kb->next;
            } else {
                binding_list = kb->next;
            }
            keybind_free(kb);
            break;
        }
        prev = kb;
        kb = kb->next;
    }

    // add to head
    kb = keybind_alloc(key, mods, action, sz);
    kb->next = binding_list;
    binding_list = kb;
}

void keybind_create(void)
{
    // set defaults
    for (size_t i = 0; i < ARRAY_SIZE(default_bindings); ++i) {
        const struct keybind_default* kb = &default_bindings[i];
        keybind_put(kb->key, kb->mods, &kb->action, 1);
    }

    // register configuration loader
    config_add_loader("keys", keybind_configure);
    config_add_loader("mouse", keybind_configure);
}

void keybind_destroy(void)
{
    struct keybind* it = binding_list;
    while (it) {
        struct keybind* next = it->next;
        keybind_free(it);
        it = next;
    }
    binding_list = NULL;
}

struct keybind* keybind_all(void)
{
    return binding_list;
}

enum config_status keybind_configure(const char* key, const char* value)
{
    xkb_keysym_t keysym;
    uint8_t mods;
    struct action action[32];
    struct str_slice action_sl[ARRAY_SIZE(action)];
    size_t action_num;

    // parse keyboard shortcut
    if (!parse_key(key, &keysym, &mods)) {
        return cfgst_invalid_key;
    }

    // parse actions
    action_num = str_split(value, ';', action_sl, ARRAY_SIZE(action_sl));
    if (action_num == 0) {
        return cfgst_invalid_value;
    }
    for (size_t i = 0; i < action_num; ++i) {
        struct str_slice* s = &action_sl[i];
        if (!action_load(&action[i], s->value, s->len)) {
            return cfgst_invalid_value;
        }
    }

    keybind_put(keysym, mods, action, action_num);

    for (size_t i = 0; i < action_num; ++i) {
        free(action[i].params);
    }

    return cfgst_ok;
}

struct keybind* keybind_get(xkb_keysym_t key, uint8_t mods)
{
    struct keybind* it = binding_list;

    // we always use lowercase + Shift modifier
    key = xkb_keysym_to_lower(key);

    while (it) {
        if (it->key == key && it->mods == mods) {
            return it;
        }
        it = it->next;
    }

    return NULL;
}

char* keybind_name(xkb_keysym_t key, uint8_t mods)
{
    char key_name[32];
    char* name = NULL;

    // skip modifiers
    switch (key) {
        case XKB_KEY_Super_L:
        case XKB_KEY_Super_R:
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
