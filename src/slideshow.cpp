// SPDX-License-Identifier: MIT
// Slide show mode.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "slideshow.hpp"

#include "application.hpp"
#include "defaults.hpp"

Slideshow& Slideshow::self()
{
    static Slideshow singleton;
    return singleton;
}

Slideshow::Slideshow()
    : duration(Defaults::slideshow::duration)
{
    default_scale = Defaults::slideshow::scale;
    window_bkg = Defaults::slideshow::window_bkg;
    image_pool.history.capacity = Defaults::slideshow::history;

    text_scheme[static_cast<size_t>(Text::TopLeft)].assign(
        Defaults::slideshow::text_scheme_tl.begin(),
        Defaults::slideshow::text_scheme_tl.end());

    Defaults::slideshow::bind_inputs(this);
}

void Slideshow::initialize()
{
    Viewer::initialize();

    Application::self().add_fdpoll(timer, [this]() {
        if (open(ImageList::Dir::Next)) {
            timer.reset(duration, 0);
        }
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
