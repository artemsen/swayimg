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

/** Key binding list. */
struct keybind {
    struct keybind* next;   ///< Pointer to the next keybind
    xkb_keysym_t key;       ///< Keyboard key
    uint8_t mods;           ///< Key modifiers
    struct action* actions; ///< Sequence of action
    char* help;             ///< Help line with binding description
};

/**
 * Load key binding list from configuration.
 * @param cfg config instance
 * @param section name of the config section
 * @return pointer to list head or NULL on errors
 */
struct keybind* keybind_load(const struct config* cfg, const char* section);

/**
 * Free key binding list.
 * @param kb head of key binding list
 */
void keybind_free(struct keybind* kb);

/**
 * Find binding for the key/mods.
 * @param kb head of key binding list
 * @param key keyboard key
 * @param mods key modifiers (ctrl/alt/shift)
 * @return pointer to key binding or NULL if not found
 */
const struct keybind* keybind_find(const struct keybind* kb, xkb_keysym_t key,
                                   uint8_t mods);

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
