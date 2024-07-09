// SPDX-License-Identifier: MIT
// Keyboard bindings.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "action.h"
#include "config.h"

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

/** Key binding list entry. */
struct keybind {
    xkb_keysym_t key;       ///< Keyboard key
    uint8_t mods;           ///< Key modifiers
    struct action* actions; ///< Array of action
    size_t num_actions;     ///< Number of action in array
    char* help;             ///< Binding description
    struct keybind* next;   ///< Next entry in list
};

/**
 * Create global default key binding scheme.
 */
void keybind_create(void);

/**
 * Destroy global key binding scheme.
 */
void keybind_destroy(void);

/**
 * Get head of the global binding list.
 * @return pointer to the list head
 */
struct keybind* keybind_all(void);

/**
 * Configure global key bindings, see `config_loader` for details.
 */
enum config_status keybind_configure(const char* key, const char* value);

/**
 * Get list of action for specified key.
 * @param key keyboard key
 * @param mods key modifiers (ctrl/alt/shift)
 * @return list of actions, last is action_none, NULL if not found
 */
struct keybind* keybind_get(xkb_keysym_t key, uint8_t mods);

/**
 * Get key name.
 * @param key keyboard key
 * @param mods key modifiers (ctrl/alt/shift)
 * @return text name of key, caller should free the buffer
 */
char* keybind_name(xkb_keysym_t key, uint8_t mods);

/**
 * Get current key modifiers state.
 * @param state XKB handle
 * @return active key modifiers (ctrl/alt/shift)
 */
uint8_t keybind_mods(struct xkb_state* state);
