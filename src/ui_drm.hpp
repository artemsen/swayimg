// SPDX-License-Identifier: MIT
// DRM based user interface.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "ui.hpp"

/** DRM based user interface. */
class UiDrm : public Ui {
public:
    UiDrm(const EventHandler& handler);

    /**
     * Initialize DRM UI.
     * @return true on success
     */
    bool initialize();

    // Implementation of UI generic interface
    size_t get_width() override;
    size_t get_height() override;
    Pixmap& lock_surface() override;
    void commit_surface() override;

private:
};
