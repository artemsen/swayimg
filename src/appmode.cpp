// SPDX-License-Identifier: MIT
// Generic application mode interface.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "appmode.hpp"

#include "log.hpp"

bool AppMode::handle_keyboard(const InputKeyboard& input)
{
    const auto& bind = kbindings.find(input);
    if (bind != kbindings.end()) {
        bind->second();
        return true;
    }
    return false;
}

bool AppMode::handle_mclick(const InputMouse& input)
{
    const auto& bind = mbindings.find(input);
    if (bind != mbindings.end()) {
        bind->second();
        return true;
    }
    return false;
}

void AppMode::handle_mmove(const InputMouse&)
{
    // ignore
}

bool AppMode::handle_signal(const InputSignal& input)
{
    const auto& bind = sbindings.find(input);
    if (bind != sbindings.end()) {
        bind->second();
        return true;
    }
    return false;
}

void AppMode::bind_reset()
{
    Log::debug("Reset all bindins");
    kbindings.clear();
    mbindings.clear();
    sbindings.clear();
}

void AppMode::bind_input(const InputKeyboard& input,
                         const InputCallback& handler)
{
    auto it = kbindings.find(input);
    if (it == kbindings.end()) {
        kbindings.insert(std::make_pair(input, handler));
    } else {
        Log::debug("Rebind existing key {}", input.to_string());
        it->second = handler;
    }
}

void AppMode::bind_input(const InputMouse& input, const InputCallback& handler)
{
    auto it = mbindings.find(input);
    if (it == mbindings.end()) {
        mbindings.insert(std::make_pair(input, handler));
    } else {
        Log::debug("Rebind existing mouse button {}", input.to_string());
        it->second = handler;
    }
}

void AppMode::bind_input(const InputSignal& input, const InputCallback& handler)
{
    auto it = sbindings.find(input);
    if (it == sbindings.end()) {
        sbindings.insert(std::make_pair(input, handler));
    } else {
        Log::debug("Rebind existing signal {}", input.to_string());
        it->second = handler;
    }
}
