// SPDX-License-Identifier: MIT
// Keyboard bindings.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "action.h"

#include <xkbcommon/xkbcommon.h>

// Key modifiers
#define KEYMOD_CTRL  (1 << 0)
#define KEYMOD_ALT   (1 << 1)
#define KEYMOD_SHIFT (1 << 2)

// Virtual keys used for scrolling (mouse wheel, touchpad etc)
#define VKEY_SCROLL_UP    0x42000001
#define VKEY_SCROLL_DOWN  0x42000002
#define VKEY_SCROLL_LEFT  0x42000003
#define VKEY_SCROLL_RIGHT 0x42000004

/** Key binding. */
struct key_binding {
    xkb_keysym_t key;       ///< Keyboard key
    uint8_t mods;           ///< Key modifiers
    struct action* actions; ///< Array of action, last element is action_none
    char* help;             ///< Binding description
};
extern struct key_binding* key_bindings;
extern size_t key_bindings_size;

/**
 * Initialize default key bindings.
 */
void keybind_create(void);

/**
 * Free key binding context.
 */
void keybind_free(void);

/**
 * Get current key modifiers state.
 * @param state XKB handle
 * @return active key modifiers (ctrl/alt/shift)
 */
uint8_t keybind_mods(struct xkb_state* state);

/**
 * Get key name.
 * @param key keyboard key
 * @param mods key modifiers (ctrl/alt/shift)
 * @return text name of key, caller should free the buffer
 */
char* keybind_name(xkb_keysym_t key, uint8_t mods);

/**
 * Get list of action for specified key.
 * @param key keyboard key
 * @param mods key modifiers (ctrl/alt/shift)
 * @return pointer to the action or NULL if not found
 */
const struct action* keybind_actions(xkb_keysym_t key, uint8_t mods);
