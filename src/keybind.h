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
#define VKEY_MOUSE_LEFT   0x42000005
#define VKEY_MOUSE_RIGHT  0x42000006
#define VKEY_MOUSE_MIDDLE 0x42000007
#define VKEY_MOUSE_SIDE   0x42000008
#define VKEY_MOUSE_EXTRA  0x42000009

/** Key binding list entry. */
struct keybind {
    struct list list;       ///< Links to prev/next entry
    xkb_keysym_t key;       ///< Keyboard key
    uint8_t mods;           ///< Key modifiers
    struct action* actions; ///< Sequence of action
    char* help;             ///< Help line with binding description
};

/**
 * Initialize global default key binding scheme.
 * @param cfg config instance
 */
void keybind_init(const struct config* cfg);

/**
 * Destroy global key binding scheme.
 */
void keybind_destroy(void);

/**
 * Get head of the global binding list.
 * @return pointer to the list head
 */
struct keybind* keybind_get(void);

/**
 * Find binding for the key.
 * @param key keyboard key
 * @param mods key modifiers (ctrl/alt/shift)
 * @return pointer to key binding or NULL if not found
 */
struct keybind* keybind_find(xkb_keysym_t key, uint8_t mods);

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
