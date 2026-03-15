// SPDX-License-Identifier: MIT
// Slide show mode.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "viewer.hpp"

class Slideshow : public Viewer {
public:
    /**
     * Get global instance of slideshow.
     * @return slideshow instance
     */
    static Slideshow& self();

    Slideshow();

    // app mode interface implementation
    void initialize() override;
    void activate(const ImageEntryPtr& entry, const Size& wnd) override;
    void deactivate() override;

public:
    size_t duration = 5000; ///< Image display time (ms)

private:
    FdTimer timer; ///< Slideshow timer
};
