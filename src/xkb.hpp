// SPDX-License-Identifier: MIT
// X keyboard extension wrapper.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "fdevent.hpp"

#include <xkbcommon/xkbcommon.h>

#include <string>

// Key modifiers
using keymod_t = uint8_t;

constexpr keymod_t KEYMOD_NONE = 0;
constexpr keymod_t KEYMOD_CTRL = 1 << 0;
constexpr keymod_t KEYMOD_ALT = 1 << 1;
constexpr keymod_t KEYMOD_SHIFT = 1 << 2;

/** X keyboard extension wrapper. */
class Xkb {
public:
    Xkb();
    ~Xkb();

    /**
     * Setup keyboard mapping.
     * @param fd keymap file descriptor
     * @param size keymap size, in bytes
     */
    void setup_mapping(const int fd, const size_t size);

    /**
     * Setup repeat rate and delay.
     * @param rate the rate of repeating keys in characters per second
     * @param delay delay in milliseconds since key down until repeating starts
     */
    void setup_repeat(const size_t rate, const size_t delay);

    /**
     * Start repeat mode.
     * @param code key code to repeat
     */
    void start_repeat(const xkb_keycode_t code);

    /**
     * Stop repeat mode.
     */
    void stop_repeat() const;

    /**
     * Get repeat description.
     * @return tuple with key code and number of repeats
     */
    std::tuple<xkb_keysym_t, size_t> get_repeat() const;

    /**
     * Get repeat file descriptor.
     * @return repeat file descriptor
     */
    int repeat_fd() const;

    /**
     * Update modifiers state.
     * @param depressed depressed modifiers
     * @param latched latched modifiers
     * @param locked locked modifiers
     * @param layout keyboard layout
     */
    void update_modifiers(const xkb_mod_mask_t depressed,
                          const xkb_mod_mask_t latched,
                          const xkb_mod_mask_t locked,
                          const xkb_layout_index_t layout);

    /**
     * Get current modifiers state.
     * @return mask with active modifiers
     */
    keymod_t get_modifiers() const;

    /**
     * Get keyboard symbol from its code.
     * @param code keyboard code
     * @return keyboard symbol
     */
    xkb_keysym_t get_keysym(const xkb_keycode_t code);

    /**
     * Check if specified key is modifier.
     * @param key keyboard key
     * @return true if specified key is modifier
     */
    static bool is_modifier(const xkb_keysym_t key);

    /**
     * Get key text description.
     * @param key keyboard key
     * @return text description of key
     */
    static std::string to_string(const xkb_keysym_t key);

    /**
     * Get key from its text description.
     * @param name text key description
     * @return keyboard key
     */
    static xkb_keysym_t from_string(const std::string& name);

private:
    // Repeat stuff
    FdTimer repeat_timer;
    size_t repeat_rate = 0;
    size_t repeat_delay = 0;
    xkb_keysym_t repeat_key = XKB_KEY_NoSymbol;

    // X keyboard extension handles
    xkb_context* context = nullptr;
    xkb_keymap* keymap = nullptr;
    xkb_state* state = nullptr;
};
