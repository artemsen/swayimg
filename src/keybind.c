// SPDX-License-Identifier: MIT
// Keyboard bindings.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#include "keybind.h"

#include "application.h"
#include "array.h"

#include <stdlib.h>
#include <string.h>

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

/** Head of global key binding list. */
static struct keybind* kb_viewer;
static struct keybind* kb_gallery;

/**
 * Convert text name to key code with modifiers.
 * @param name key text name
 * @param key output keyboard key
 * @param mods output key modifiers (ctrl/alt/shift)
 * @return false if name is invalid
 */
static bool parse_keymod(const char* name, xkb_keysym_t* key, uint8_t* mods)
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
        for (size_t i = 0; i < ARRAY_SIZE(mause_keys); ++i) {
            if (strcmp(slices[snum - 1].value, mause_keys[i].name) == 0) {
                *key = MOUSE_TO_XKB(mause_keys[i].btn);
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
 * @param actions sequence of actions
 */
static struct keybind* create_binding(xkb_keysym_t key, uint8_t mods,
                                      struct action* actions)
{
    struct keybind* kb = calloc(1, sizeof(struct keybind));
    if (!kb) {
        return NULL;
    }

    kb->key = key;
    kb->mods = mods;
    kb->actions = actions;

    // construct help description
    if (kb->actions->type != action_none) {
        size_t max_len = 30;
        char* key_name = keybind_name(kb->key, kb->mods);
        if (key_name) {
            const struct action* action = kb->actions; // first only
            str_append(key_name, 0, &kb->help);
            str_append(": ", 0, &kb->help);
            str_append(action_typename(action), 0, &kb->help);
            if (*action->params) {
                str_append(" ", 0, &kb->help);
                str_append(action->params, 0, &kb->help);
            }
            if (kb->actions->next) {
                str_append("; ...", 0, &kb->help);
            }
            if (strlen(kb->help) > max_len) {
                const char* ellipsis = "...";
                const size_t ellipsis_len = strlen(ellipsis);
                memcpy(&kb->help[max_len - ellipsis_len], ellipsis,
                       ellipsis_len + 1);
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
static void free_binding(struct keybind* kb)
{
    if (kb) {
        action_free(kb->actions);
        free(kb->help);
        free(kb);
    }
}

/**
 * Put key binding to the global scheme.
 * @param kb head of binding list
 * @param key keyboard key
 * @param mods key modifiers (ctrl/alt/shift)
 * @param actions sequence of actions
 */
static void set_binding(struct keybind** head, xkb_keysym_t key, uint8_t mods,
                        struct action* actions)
{
    struct keybind* kb;

    // remove existing binding
    list_for_each(*head, struct keybind, it) {
        if (it->key == key && it->mods == mods) {
            *head = list_remove(&it->list);
            free_binding(it);
            break;
        }
    }

    // create new binding
    kb = create_binding(key, mods, actions);
    if (kb) {
        *head = list_add(*head, kb);
    } else {
        action_free(actions);
    }
}

/**
 * Load binding from config parameters.
 * @param kb head of binding list
 * @param cfg config instance
 * @param section name of the section
 * @param value actions
 */
static void load_binding(struct keybind** head, const struct config* cfg,
                         const char* section)
{
    list_for_each(cfg, const struct config, cs) {
        if (strcmp(section, cs->name) == 0) {
            list_for_each(cs->params, const struct config_keyval, kv) {
                struct action* actions;
                xkb_keysym_t keysym;
                uint8_t mods;

                // parse keyboard shortcut
                if (!parse_keymod(kv->key, &keysym, &mods)) {
                    config_error_key(section, kv->key);
                    continue;
                }
                // parse actions
                actions = action_create(kv->value);
                if (!actions) {
                    config_error_val(section, kv->value);
                    continue;
                }

                set_binding(head, keysym, mods, actions);
            }
            break;
        }
    }
}

void keybind_init(const struct config* cfg)
{
    load_binding(&kb_viewer, cfg, CFG_KEYS_VIEWER);
    load_binding(&kb_gallery, cfg, CFG_KEYS_GALLERY);
}

void keybind_destroy(void)
{
    list_for_each(kb_viewer, struct keybind, it) {
        free_binding(it);
    }
    kb_viewer = NULL;
    list_for_each(kb_gallery, struct keybind, it) {
        free_binding(it);
    }
    kb_gallery = NULL;
}

struct keybind* keybind_get(void)
{
    return app_is_viewer() ? kb_viewer : kb_gallery;
}

struct keybind* keybind_find(xkb_keysym_t key, uint8_t mods)
{
    // we always use lowercase + Shift modifier
    key = xkb_keysym_to_lower(key);

    list_for_each(keybind_get(), struct keybind, it) {
        if (it->key == key && it->mods == mods) {
            return it;
        }
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
