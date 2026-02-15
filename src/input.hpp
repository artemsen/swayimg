// SPDX-License-Identifier: MIT
// User input bindings.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "xkb.hpp"

#include <optional>
#include <string>

/** Description of key with modifiers. */
struct InputKeyboard {
    /**
     * Construct key combination from text description.
     * @param expression text key description
     * @return input handle
     */
    static std::optional<InputKeyboard> load(const std::string& expression);

    /**
     * Get key combination description.
     * @return text description of key combination
     */
    std::string to_string() const;

    /** Compare operator. */
    bool operator<(const InputKeyboard& other) const;

    xkb_keysym_t key = XKB_KEY_NoSymbol; ///< Keyboard key
    keymod_t mods = KEYMOD_NONE;         ///< Keyboard modifiers
};

struct InputMouse {
    using mouse_btn_t = uint16_t;

    /**
     * Construct mouse state from text description.
     * @param expression text key description
     * @return input handle
     */
    static std::optional<InputMouse> load(const std::string& expression);

    // * The button is a button code as defined in the Linux kernel's
    // * linux/input-event-codes.h header file, e.g. BTN_LEFT.
    static mouse_btn_t to_button(uint16_t code);

    /**
     * Get buttons combination description.
     * @return text description of buttons combination
     */
    std::string to_string() const;

    /** Compare operator. */
    bool operator<(const InputMouse& other) const;

    mouse_btn_t buttons = NONE;  ///< Mouse buttons
    keymod_t mods = KEYMOD_NONE; ///< Keyboard modifiers
    size_t x, y;                 ///< Mouse pointer coordinates

    // Button identifiers
    static constexpr mouse_btn_t NONE = 0;
    static constexpr mouse_btn_t BUTTON_LEFT = 1 << 0;
    static constexpr mouse_btn_t BUTTON_RIGHT = 1 << 1;
    static constexpr mouse_btn_t BUTTON_MIDDLE = 1 << 2;
    static constexpr mouse_btn_t BUTTON_SIDE = 1 << 3;
    static constexpr mouse_btn_t BUTTON_EXTRA = 1 << 4;
    static constexpr mouse_btn_t SCROLL_UP = 1 << 5;
    static constexpr mouse_btn_t SCROLL_DOWN = 1 << 6;
    static constexpr mouse_btn_t SCROLL_LEFT = 1 << 7;
    static constexpr mouse_btn_t SCROLL_RIGHT = 1 << 8;
};

struct InputSignal {
    /**
     * Construct signal state from text description.
     * @param expression text key description
     * @return input handle
     */
    static std::optional<InputSignal> load(const std::string& expression);

    /**
     * Get signal description.
     * @return text description of signal
     */
    std::string to_string() const;

    /** Compare operator. */
    bool operator<(const InputSignal& other) const;

    uint8_t signal = 0; ///< Signal number

    static constexpr uint8_t USR1 = 1;
    static constexpr uint8_t USR2 = 2;
};
