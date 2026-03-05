// SPDX-License-Identifier: MIT
// Generic application mode interface.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "appmode.hpp"

#include "application.hpp"

#include <format>

AppMode::AppMode()
{
    mark_color = { argb_t::max, 0x80, 0x80, 0x80 };
}

void AppMode::activate(const ImageEntryPtr&, const Size&)
{
    Text& text = Text::self();
    for (size_t i = 0; i < sizeof(text_scheme) / sizeof(text_scheme[0]); ++i) {
        text.set_scheme(static_cast<Text::Position>(i), text_scheme[i]);
    }
}

bool AppMode::handle_keyboard(const InputKeyboard& input)
{
    const auto& bind = kbindings.find(input);
    if (bind != kbindings.end()) {
        bind->second();
        return true;
    }
    return false;
}

bool AppMode::handle_mclick(const InputMouse& input, const Point&)
{
    const auto& bind = mbindings.find(input);
    if (bind != mbindings.end()) {
        bind->second();
        return true;
    }
    return false;
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

bool AppMode::is_active() const
{
    return Application::self().current_mode() == this;
}

void AppMode::subscribe(const ImageSwitchCallback& cb)
{
    imswitch_cb.push_back(cb);
}

void AppMode::set_mark_color(const argb_t& color)
{
    mark_color = color;
    if (is_active()) {
        Application::redraw();
    }
}

void AppMode::set_text_scheme(const Text::Position pos,
                              const std::vector<std::string>& scheme)
{
    text_scheme[static_cast<size_t>(pos)] = scheme;
    if (is_active()) {
        Text& text = Text::self();
        text.set_scheme(pos, scheme);
        text.update();
        Application::redraw();
    }
}

void AppMode::bind_reset()
{
    kbindings.clear();
    mbindings.clear();
    sbindings.clear();
}

void AppMode::bind_input(const InputKeyboard& input,
                         const InputCallback& handler)
{
    kbindings.insert_or_assign(input, handler);
}

void AppMode::bind_input(const InputMouse& input, const InputCallback& handler)
{
    mbindings.insert_or_assign(input, handler);
}

void AppMode::bind_input(const InputSignal& input, const InputCallback& handler)
{
    sbindings.insert_or_assign(input, handler);
}

void AppMode::switch_current()
{
    ImageEntryPtr current = current_entry();

    // set window title
    Ui* ui = Application::self().get_ui();
    const std::string title =
        std::format("Swayimg: {}", current->path.filename().string());
    ui->set_title(title.c_str());

    // update text layer
    Text::self().reset(current);

    // call handlers
    for (auto& it : imswitch_cb) {
        it();
    }

    Application::redraw();
}
