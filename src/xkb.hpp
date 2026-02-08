// SPDX-License-Identifier: MIT
// X keyboard extension wrapper.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "fdevent.hpp"

#include <xkbcommon/xkbcommon.h>

#include <string>

using keymod_t = uint8_t;
constexpr keymod_t KEYMOD_NONE = 0;
constexpr keymod_t KEYMOD_CTRL = 1 << 0;
constexpr keymod_t KEYMOD_ALT = 1 << 1;
constexpr keymod_t KEYMOD_SHIFT = 1 << 2;

class Xkb {
public:
    Xkb();
    ~Xkb();

    void set_mapping(int fd, size_t size);

    void setup_repeat(size_t rate, size_t delay);
    void start_repeat(xkb_keycode_t code);
    void stop_repeat() const;
    std::tuple<xkb_keysym_t, size_t> get_repeat() const;
    int repeat_fd() const;

    static bool is_modifier(xkb_keysym_t key);
    keymod_t get_modifiers() const;

    void update_modifiers(xkb_mod_mask_t depressed, xkb_mod_mask_t latched,
                          xkb_mod_mask_t locked, xkb_layout_index_t llayout);

    xkb_keysym_t get_keysym(xkb_keycode_t code);

    static std::string to_string(xkb_keysym_t key);

    /**
     * Get key text description.
     * @return text description of key
     */
    static xkb_keysym_t from_string(const std::string& name);

private:
    size_t repeat_rate = 0;
    size_t repeat_delay = 0;
    xkb_keysym_t repeat_key = XKB_KEY_NoSymbol;
    FdTimer repeat_timer;

    // X keyboard extension handles
    xkb_context* context = nullptr;
    xkb_keymap* keymap = nullptr;
    xkb_state* state = nullptr;
};
