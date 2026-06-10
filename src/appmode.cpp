// SPDX-License-Identifier: MIT
// Generic application mode interface.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "appmode.hpp"

#include "application.hpp"
#include "imagelist.hpp"

AppMode::AppMode()
{
    mark_color = { argb_t::max, 0x80, 0x80, 0x80 };
    pinch_factor = 1.0;
}

void AppMode::activate(const ImageEntryPtr&, const Size&)
{
    Text& text = Text::self();
    text.clear();
    for (size_t i = 0; i < text_scheme.size(); ++i) {
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

void AppMode::handle_imagelist(const ImageListEvent,
                               const std::list<ImageEntryPtr>&)
{
    const ImageEntryPtr entry = get_current();
    Text& text = Text::self();
    text.set_field(Text::FIELD_LIST_INDEX,
                   entry ? std::to_string(entry->index + 1) : "");
    text.set_field(Text::FIELD_LIST_TOTAL,
                   std::to_string(ImageList::self().size()));
    text.update();
}

bool AppMode::is_active() const
{
    return Application::self().current_mode() == this;
}

void AppMode::subscribe_image_switch(const ImageSwitchNotify& cb)
{
    img_switch.push_back(cb);
}

void AppMode::mark_current(const std::optional<bool>& state)
{
    const ImageEntryPtr entry = get_current();
    if (entry) {
        if (state.has_value()) {
            entry->mark = state.value();
        } else {
            entry->mark = !entry->mark;
        }
        if (is_active()) {
            Application::redraw();
        }
    }
}

void AppMode::set_mark_color(const argb_t& color)
{
    mark_color = color;
    if (is_active()) {
        Application::redraw();
    }
}

void AppMode::set_pinch_factor(const double factor)
{
    if (factor >= 0.0) {
        pinch_factor = factor;
    }
}

void AppMode::set_text_scheme(const Text::Position pos,
                              const Text::Scheme& scheme)
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
    const ImageEntryPtr entry = get_current();

    // set window title
    Ui* ui = Application::self().get_ui();
    std::string title = "Swayimg: ";
    if (entry) {
        title += entry->path.filename().string();
    } else {
        title += "no image";
    }
    ui->set_title(title.c_str());

    // update text layer
    Text& text = Text::self();
    if (entry) {
        text.reset(entry);
    } else {
        text.clear();
        text.set_status("Image list is empty");
    }

    // call handlers
    for (auto& it : img_switch) {
        it();
    }

    Application::redraw();
}
