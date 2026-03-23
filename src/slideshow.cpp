// SPDX-License-Identifier: MIT
// Slide show mode.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "slideshow.hpp"

#include "application.hpp"

Slideshow& Slideshow::self()
{
    static Slideshow singleton;
    return singleton;
}

Slideshow::Slideshow()
{
    // default settings

    set_history_limit(0);
    set_window_background(Background::Auto);
    default_scale = Scale::FitWindow;

    text_scheme[static_cast<size_t>(Text::TopRight)].clear();
    text_scheme[static_cast<size_t>(Text::TopLeft)] = { "{name}" };
    text_scheme[static_cast<size_t>(Text::BottomLeft)].clear();
    text_scheme[static_cast<size_t>(Text::BottomRight)].clear();

    bind_input(InputKeyboard { XKB_KEY_s, KEYMOD_NONE }, []() {
        Application::self().set_mode(Application::Mode::Viewer);
    });
}

void Slideshow::initialize()
{
    Viewer::initialize();

    Application::self().add_fdpoll(timer, [this]() {
        open(ImageList::Dir::Next);
        timer.reset(duration, 0);
    });
}

void Slideshow::activate(const ImageEntryPtr& entry, const Size& wnd)
{
    Viewer::activate(entry, wnd);
    timer.reset(duration, 0);
}

void Slideshow::deactivate()
{
    Viewer::deactivate();
    timer.reset(0, 0);
}
