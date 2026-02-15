// SPDX-License-Identifier: MIT
// X keyboard extension wrapper.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "xkb.hpp"

#include <sys/mman.h>
#include <unistd.h>

#include <cassert>

Xkb::Xkb()
    : context(xkb_context_new(XKB_CONTEXT_NO_FLAGS))
{
}

Xkb::~Xkb()
{
    if (state) {
        xkb_state_unref(state);
    }
    if (keymap) {
        xkb_keymap_unref(keymap);
    }
    if (context) {
        xkb_context_unref(context);
    }
}

void Xkb::set_mapping(int fd, size_t size)
{
    void* km = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
    if (km != MAP_FAILED) {
        if (keymap) {
            xkb_keymap_unref(keymap);
            keymap = nullptr;
        }
        keymap = xkb_keymap_new_from_string(
            context, static_cast<const char*>(km), XKB_KEYMAP_FORMAT_TEXT_V1,
            XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap(km, size);

        if (state) {
            xkb_state_unref(state);
            state = nullptr;
        }
        state = xkb_state_new(keymap);
    }
}

bool Xkb::is_modifier(xkb_keysym_t key)
{
    switch (key) {
        case XKB_KEY_Shift_L:
        case XKB_KEY_Shift_R:
        case XKB_KEY_Control_L:
        case XKB_KEY_Control_R:
        case XKB_KEY_Caps_Lock:
        case XKB_KEY_Shift_Lock:
        case XKB_KEY_Meta_L:
        case XKB_KEY_Meta_R:
        case XKB_KEY_Alt_L:
        case XKB_KEY_Alt_R:
        case XKB_KEY_Super_L:
        case XKB_KEY_Super_R:
        case XKB_KEY_Hyper_L:
        case XKB_KEY_Hyper_R:
            return true;
    }
    return false;
}

keymod_t Xkb::get_modifiers() const
{
    assert(state);

    keymod_t mods = KEYMOD_NONE;
    if (state) {
        if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_CTRL,
                                         XKB_STATE_MODS_EFFECTIVE) > 0) {
            mods |= KEYMOD_CTRL;
        }
        if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_ALT,
                                         XKB_STATE_MODS_EFFECTIVE) > 0) {
            mods |= KEYMOD_ALT;
        }
        if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_SHIFT,
                                         XKB_STATE_MODS_EFFECTIVE) > 0) {
            mods |= KEYMOD_SHIFT;
        }
    }
    return mods;
}

void Xkb::update_modifiers(xkb_mod_mask_t depressed, xkb_mod_mask_t latched,
                           xkb_mod_mask_t locked, xkb_layout_index_t llayout)
{
    assert(state);
    xkb_state_update_mask(state, depressed, latched, locked, 0, 0, llayout);
}

xkb_keysym_t Xkb::get_keysym(xkb_keycode_t code)
{
    assert(state);
    return xkb_state_key_get_one_sym(state, code);
}

void Xkb::setup_repeat(size_t rate, size_t delay)
{
    repeat_rate = rate;
    repeat_delay = delay;
}

int Xkb::repeat_fd() const
{
    return repeat_timer;
}

void Xkb::start_repeat(xkb_keycode_t code)
{
    if (repeat_rate && xkb_keymap_key_repeats(keymap, code)) {
        // start key repeat timer
        repeat_key = xkb_state_key_get_one_sym(state, code);
        repeat_timer.reset(repeat_delay, 1000 / repeat_rate);
    }
}

void Xkb::stop_repeat() const
{
    repeat_timer.reset(0, 0);
}

std::string Xkb::to_string(xkb_keysym_t key)
{
    const xkb_keysym_t lower = xkb_keysym_to_lower(key);
    char buf[32];
    if (xkb_keysym_get_name(lower, buf, sizeof(buf)) > 0) {
        return buf;
    }
    return "<UNKNOWN>";
}

std::tuple<xkb_keysym_t, size_t> Xkb::get_repeat() const
{
    size_t count = 0;

    uint64_t val;
    if (read(repeat_timer, &val, sizeof(val)) == sizeof(val)) {
        ++count;
    }

    return std::make_tuple(repeat_key, count);
}

xkb_keysym_t Xkb::from_string(const std::string& name)
{
    xkb_keysym_t key =
        xkb_keysym_from_name(name.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);

    // check for international symbols
    if (key == XKB_KEY_NoSymbol) {
        std::wstring wide;
        wide.resize(name.length() + 8);
        const size_t len = std::mbstowcs(wide.data(), name.c_str(),
                                         wide.size() * sizeof(wide[0]));
        if (len == 1) {
            key = xkb_utf32_to_keysym(wide[0]);
        }
    }

    return key;
}
