// SPDX-License-Identifier: MIT
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <cairo/cairo.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * Create Cairo surface.
 * @param[in] width image width in px
 * @param[in] height image height in px
 * @param[in] alpha true if image has alpha channel
 * @return surface pointer or NULL on errors
 */
cairo_surface_t* create_surface(size_t width, size_t height, bool alpha);

/**
 * Apply alpha channel.
 * @param[in] surface surface to apply
 */
void apply_alpha(cairo_surface_t* surface);
