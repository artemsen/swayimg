// SPDX-License-Identifier: MIT
// Gallery mode.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "gallery.hpp"

void Gallery::initialize()
{
    // TODO
}

void Gallery::activate(ImageList::EntryPtr entry)
{
    // TODO
    (void)entry;
}

void Gallery::deactivate()
{
    // TODO
}

void Gallery::reset()
{
    // TODO
}

ImageList::EntryPtr Gallery::current_image()
{
    // TODO
    return nullptr;
}

void Gallery::window_resize()
{
    // TODO
}

void Gallery::window_redraw(Pixmap& wnd)
{
    // TODO
    (void)wnd;
}

void Gallery::handle_mmove(const InputMouse& input)
{
    // TODO
    (void)input;
}

void Gallery::handle_imagelist(const FsMonitor::Event event,
                               const ImageList::EntryPtr& entry)
{
    // TODO
    (void)event;
    (void)entry;
}
