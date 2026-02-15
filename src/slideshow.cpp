// SPDX-License-Identifier: MIT
// Slide show mode.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "slideshow.hpp"

Slideshow::Slideshow()
{
    history_limit = 0;
    bkg_mode = Background::Auto;
}
