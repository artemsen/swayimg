// SPDX-License-Identifier: MIT
// Gallery mode.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "appmode.hpp"

class Gallery : public AppMode {
public:
    // app mode interface implementation
    void initialize() override;
    void activate(ImageList::EntryPtr entry) override;
    void deactivate() override;
    void reset() override;
    ImageList::EntryPtr current_image() override;
    void window_resize() override;
    void window_redraw(Pixmap& wnd) override;
    void handle_mmove(const InputMouse& input) override;
    void handle_imagelist(const FsMonitor::Event event,
                          const ImageList::EntryPtr& entry) override;
};
