// SPDX-License-Identifier: MIT
// Keyboard bindings.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#include "keybind.h"

#include "array.h"

#include <stdlib.h>
#include <string.h>

// Special id for invalid key modifiers state
#define KEYMOD_INVALID 0xff

// Names of virtual mouse buttons/scroll
struct mouse_keys {
    uint16_t btn;
    const char* name;
};
static const struct mouse_keys mause_keys[] = {
    { MOUSE_BTN_LEFT,   "MouseLeft"   },
    { MOUSE_BTN_RIGHT,  "MouseRight"  },
    { MOUSE_BTN_MIDDLE, "MouseMiddle" },
    { MOUSE_BTN_SIDE,   "MouseSide"   },
    { MOUSE_BTN_EXTRA,  "MouseExtra"  },
    { MOUSE_SCR_UP,     "ScrollUp"    },
    { MOUSE_SCR_DOWN,   "ScrollDown"  },
    { MOUSE_SCR_LEFT,   "ScrollLeft"  },
    { MOUSE_SCR_RIGHT,  "ScrollRight" },
};

/**
 * Parse config line to keyboard modifiers (ctrl/alt/shift).
 * @param conf config line to parse
 * @return key modifiers (ctrl/alt/shift) or KEYMOD_INVALID on format error
 */
static uint8_t parse_mod(const char* conf)
{
    uint8_t mod = 0;
    struct str_slice slices[4]; // mod[alt+ctrl+shift]+key
    size_t snum;

    snum = str_split(conf, '+', slices, ARRAY_SIZE(slices));
    if (snum == 0) {
        return 0;
    }

    for (size_t i = 0; i < snum - 1; ++i) {
        const char* mod_names[] = { "Ctrl", "Alt", "Shift" };
        const ssize_t index =
            str_index(mod_names, slices[i].value, slices[i].len);
        if (index < 0) {
            return KEYMOD_INVALID;
        }
        mod |= 1 << index;
    }

    return mod;
}

/**
 * Parse config line to key code.
 * @param conf config line to parse
 * @return key code or XKB_KEY_NoSymbol on format error
 */
static xkb_keysym_t parse_key(const char* conf)
{
    const char* name;
    xkb_keysym_t key;

    // skip modifiers
    name = strrchr(conf, '+');
    if (name) {
        ++name; // skip '+'
    } else {
        name = conf;
    }

    // get key
    key = xkb_keysym_from_name(name, XKB_KEYSYM_CASE_INSENSITIVE);

    // check for virtual keys
    if (key == XKB_KEY_NoSymbol) {
        for (size_t i = 0; i < ARRAY_SIZE(mause_keys); ++i) {
            if (strcmp(name, mause_keys[i].name) == 0) {
                key = MOUSE_TO_XKB(mause_keys[i].btn);
                break;
            }
        }
    }

    // check for international symbols
    if (key == XKB_KEY_NoSymbol) {
        wchar_t* wide = str_to_wide(name, NULL);
        if (wide) {
            key = xkb_utf32_to_keysym(wide[0]);
            free(wide);
        }
    }

    return key;
}

/**
 * Construct help line.
 * @param key keyboard key
 * @param mods key modifiers (ctrl/alt/shift)
 * @param action sequence of actions
 * @return help line or NULL if not applicable
 */
static char* help_line(xkb_keysym_t key, uint8_t mods,
                       const struct action* action)
{
    const size_t max_len = 30;
    char* help;

    help = keybind_name(key, mods);
    if (!help) {
        return NULL;
    }

    str_append(": ", 0, &help);
    str_append(action_typename(action), 0, &help);
    if (*action->params) {
        str_append(" ", 0, &help);
        str_append(action->params, 0, &help);
    }
    if (action->next) {
        str_append("; ...", 0, &help);
    }
    if (strlen(help) > max_len) {
        const char* ellipsis = "...";
        const size_t ellipsis_len = strlen(ellipsis);
        memcpy(&help[max_len - ellipsis_len], ellipsis, ellipsis_len + 1);
    }

    return help;
}

/**
 * Register key binding.
 * @param kb keybind to add
 * @param head head of binding list
 * @return pointer to created binding
 */
static struct keybind* set_binding(const struct keybind* kb,
                                   struct keybind* head)
{
    struct keybind* new;
    struct keybind* it;

    // search for existing binding with the same key/mods
    it = head;
    while (it) {
        if (it->key == kb->key && it->mods == kb->mods) {
            action_free(it->actions);
            free(it->help);
            it->actions = kb->actions;
            it->help = kb->help;
            return it;
        }
        it = it->next;
    }

    // create new entry
    new = malloc(sizeof(*new));
    if (!new) {
        return NULL;
    }
    *new = *kb;

    // add to tail
    it = head;
    if (it) {
        while (it->next) {
            it = it->next;
        }
        it->next = new;
    }

    return new;
}

struct keybind* keybind_load(const struct config* section)
{
    struct keybind* head;
    const struct config_keyval* kv;

    if (!section) {
        return NULL;
    }

    // load scheme
    head = NULL;
    kv = section->params;
    while (kv) {
        struct keybind kb = {
            .key = parse_key(kv->key),
            .mods = parse_mod(kv->key),
            .actions = action_create(kv->value),
        };

        if (!kb.actions) {
            config_error_val(section->name, kv->value);
        } else if (kb.key == XKB_KEY_NoSymbol || kb.mods == KEYMOD_INVALID) {
            action_free(kb.actions);
            config_error_key(section->name, kv->key);
        } else {
            struct keybind* new;
            kb.help = help_line(kb.key, kb.mods, kb.actions);
            new = set_binding(&kb, head);
            if (!head) {
                head = new;
            }
        }

        kv = kv->next;
    }

    return head;
}

void keybind_free(struct keybind* kb)
{
    while (kb) {
        struct keybind* next = kb->next;
        action_free(kb->actions);
        free(kb->help);
        free(kb);
        kb = next;
    }
}

const struct keybind* keybind_find(const struct keybind* kb, xkb_keysym_t key,
                                   uint8_t mods)
{
    // we always use lowercase + Shift modifier
    key = xkb_keysym_to_lower(key);

    while (kb) {
        if (kb->key == key && kb->mods == mods) {
            return kb;
        }
        kb = kb->next;
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

    // modifiers
    if (mods & KEYMOD_CTRL) {
        str_append("Ctrl+", 0, &name);
    }
    if (mods & KEYMOD_ALT) {
        str_append("Alt+", 0, &name);
    }
    if (mods & KEYMOD_SHIFT) {
        str_append("Shift+", 0, &name);
    }

    // key name
    if ((MOUSE_XKB_BASE & key) == MOUSE_XKB_BASE) {
        const uint32_t btn = XKB_TO_MOUSE(key);
        for (size_t i = 0; i < ARRAY_SIZE(mause_keys); ++i) {
            if (mause_keys[i].btn & btn) {
                if (name && name[strlen(name) - 1] != '+') {
                    str_append("+", 0, &name);
                }
                str_append(mause_keys[i].name, 0, &name);
            }
        }
    } else {
        key = xkb_keysym_to_lower(key);
        if (xkb_keysym_get_name(key, key_name, sizeof(key_name)) > 0) {
            str_append(key_name, 0, &name);
        }
    }

    if (!name) {
        name = str_dup("<UNKNOWN>", NULL);
    }

    return name;
}

uint8_t keybind_mods(struct xkb_state* state)
{
    uint8_t mods = 0;

    if (state) {
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
    }

    return mods;
}
