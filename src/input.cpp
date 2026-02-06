// SPDX-License-Identifier: MIT
// User input bindings.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "input.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <vector>

// Mouse buttons, from <linux/input-event-codes.h>
#ifndef BTN_LEFT
#define BTN_LEFT   0x110
#define BTN_RIGHT  0x111
#define BTN_MIDDLE 0x112
#define BTN_SIDE   0x113
#define BTN_EXTRA  0x114
#endif

static constexpr std::array modifiers_name =
    std::to_array<std::pair<keymod_t, const char*>>({
        { KEYMOD_CTRL,  "Ctrl"  },
        { KEYMOD_ALT,   "Alt"   },
        { KEYMOD_SHIFT, "Shift" },
});

static constexpr std::array mouse_buttons =
    std::to_array<std::pair<InputMouse::mouse_btn_t, const char*>>({
        { InputMouse::BUTTON_LEFT,   "MouseLeft"   },
        { InputMouse::BUTTON_RIGHT,  "MouseRight"  },
        { InputMouse::BUTTON_MIDDLE, "MouseMiddle" },
        { InputMouse::BUTTON_SIDE,   "MouseSide"   },
        { InputMouse::BUTTON_EXTRA,  "MouseExtra"  },
        { InputMouse::SCROLL_UP,     "ScrollUp"    },
        { InputMouse::SCROLL_DOWN,   "ScrollDown"  },
        { InputMouse::SCROLL_LEFT,   "ScrollLeft"  },
        { InputMouse::SCROLL_RIGHT,  "ScrollRight" },
});

static std::string modifiers_to_string(keymod_t mods)
{
    std::string name;
    for (const auto& it : modifiers_name) {
        if (mods & it.first) {
            if (!name.empty()) {
                name += '+';
            }
            name += it.second;
        }
    }
    return name;
}

static keymod_t modifiers_from_string(std::vector<std::string>& tokens)
{
    keymod_t mods = KEYMOD_NONE;
    for (auto it = tokens.begin(); it != tokens.end();) {
        const std::string& name = *it;
        const auto kit =
            std::find_if(modifiers_name.begin(), modifiers_name.end(),
                         [name](const std::pair<uint8_t, const char*>& it) {
                             return it.second == name;
                         });
        if (kit == modifiers_name.end()) {
            ++it;
        } else {
            mods |= kit->first;
            it = tokens.erase(it);
        }
    }
    return mods;
}

static std::vector<std::string> split(const std::string& text)
{
    const std::string delimiters = "+- ";
    std::vector<std::string> tokens;

    size_t last = text.find_first_not_of(delimiters, 0);
    size_t pos = text.find_first_of(delimiters, last);

    while (pos != std::string::npos || last != std::string::npos) {
        tokens.push_back(text.substr(last, pos - last));
        last = text.find_first_not_of(delimiters, pos);
        pos = text.find_first_of(delimiters, last);
    }

    return tokens;
}

std::optional<InputKeyboard> InputKeyboard::load(const std::string& expression)
{
    std::vector<std::string> tokens = split(expression);

    const keymod_t mods = modifiers_from_string(tokens);
    if (tokens.size() != 1) {
        return std::nullopt;
    }

    const xkb_keysym_t key = Xkb::from_string(tokens[0]);
    if (key == XKB_KEY_NoSymbol) {
        return std::nullopt;
    }

    return InputKeyboard { key, mods };
}

std::string InputKeyboard::to_string() const
{
    std::string text = modifiers_to_string(mods);
    if (key != XKB_KEY_NoSymbol) {
        if (!text.empty()) {
            text += '+';
        }
        text += Xkb::to_string(key);
    }
    return text;
}

bool InputKeyboard::operator<(const InputKeyboard& other) const
{
    return std::tie(key, mods) < std::tie(other.key, other.mods);
}

std::optional<InputMouse> InputMouse::load(const std::string& expression)
{
    std::vector<std::string> tokens = split(expression);
    const keymod_t mods = modifiers_from_string(tokens);

    InputMouse::mouse_btn_t buttons = InputMouse::NONE;
    for (auto it = tokens.begin(); it != tokens.end();) {
        const std::string& name = *it;
        const auto btnit = std::find_if(
            mouse_buttons.begin(), mouse_buttons.end(),
            [name](const std::pair<InputMouse::mouse_btn_t, const char*>& it) {
                return it.second == name;
            });
        if (btnit == mouse_buttons.end()) {
            ++it;
        } else {
            buttons |= btnit->first;
            it = tokens.erase(it);
        }
    }
    if (buttons == InputMouse::NONE || !tokens.empty()) {
        return std::nullopt;
    }

    return InputMouse { buttons, mods, 0, 0 };
}

InputMouse::mouse_btn_t InputMouse::to_button(uint16_t code)
{
    switch (code) {
        case BTN_LEFT:
            return BUTTON_LEFT;
        case BTN_RIGHT:
            return BUTTON_RIGHT;
        case BTN_MIDDLE:
            return BUTTON_MIDDLE;
        case BTN_SIDE:
            return BUTTON_SIDE;
        case BTN_EXTRA:
            return BUTTON_EXTRA;
    }
    return NONE;
}

std::string InputMouse::to_string() const
{
    std::string text = modifiers_to_string(mods);
    if (buttons != NONE) {
        for (const auto& it : mouse_buttons) {
            if (buttons & it.first) {
                if (!text.empty()) {
                    text += '+';
                }
                text += it.second;
            }
        }
    }
    return text;
}

bool InputMouse::operator<(const InputMouse& other) const
{
    return std::tie(buttons, mods) < std::tie(other.buttons, other.mods);
}

std::optional<InputSignal> InputSignal::load(const std::string& expression)
{
    if (expression == "USR1") {
        return InputSignal(InputSignal::USR1);
    }
    if (expression == "USR2") {
        return InputSignal(InputSignal::USR2);
    }
    return std::nullopt;
}

std::string InputSignal::to_string() const
{
    switch (signal) {
        case USR1:
            return "USR1";
        case USR2:
            return "USR2";
        default:
            assert(false && "unknown sinal");
    };
    return {};
}

bool InputSignal::operator<(const InputSignal& other) const
{
    return signal < other.signal;
}
