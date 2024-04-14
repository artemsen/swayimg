// SPDX-License-Identifier: MIT
// Keyboard bindings.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#pragma once

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

/** Available actions. */
enum kb_action {
    kb_none,
    kb_help,
    kb_first_file,
    kb_last_file,
    kb_prev_dir,
    kb_next_dir,
    kb_prev_file,
    kb_next_file,
    kb_prev_frame,
    kb_next_frame,
    kb_animation,
    kb_slideshow,
    kb_fullscreen,
    kb_step_left,
    kb_step_right,
    kb_step_up,
    kb_step_down,
    kb_zoom,
    kb_rotate_left,
    kb_rotate_right,
    kb_flip_vertical,
    kb_flip_horizontal,
    kb_reload,
    kb_antialiasing,
    kb_info,
    kb_exec,
    kb_exit,
};

/** Key binding. */
struct key_binding {
    xkb_keysym_t key;      ///< Keyboard key
    uint8_t mods;          ///< Key modifiers
    enum kb_action action; ///< Action
    char* params;          ///< Custom parameters for the action
    char* help;            ///< Binding description
};
extern struct key_binding* key_bindings;
extern size_t key_bindings_size;

/**
 * Initialize default key bindings.
 */
void keybind_init(void);

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
 * Get key binding description.
 * @param key keyboard key
 * @param mods key modifiers (ctrl/alt/shift)
 * @return pointer to the binding or NULL if not found
 */
const struct key_binding* keybind_get(xkb_keysym_t key, uint8_t mods);
