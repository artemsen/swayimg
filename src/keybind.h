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

// Virtual mouse keys
#define MOUSE_BTN_LEFT   (1UL << 0)
#define MOUSE_BTN_RIGHT  (1UL << 1)
#define MOUSE_BTN_MIDDLE (1UL << 2)
#define MOUSE_BTN_SIDE   (1UL << 3)
#define MOUSE_BTN_EXTRA  (1UL << 4)
#define MOUSE_SCR_UP     (1UL << 5)
#define MOUSE_SCR_DOWN   (1UL << 6)
#define MOUSE_SCR_LEFT   (1UL << 7)
#define MOUSE_SCR_RIGHT  (1UL << 8)
#define MOUSE_XKB_BASE   0xffff0000
#define MOUSE_TO_XKB(m)  (MOUSE_XKB_BASE | (m))
#define XKB_TO_MOUSE(x)  ((x) & ~MOUSE_XKB_BASE)

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
