// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <cairo/cairo.h>

/**
 * Draw background grid for transparent images.
 * @param[in] cr cairo paint context
 * @param[in] x left offset
 * @param[in] y top offset
 * @param[in] width width of the background area
 * @param[in] height height of the background area
 * @param[in] angle image angle
 */
void draw_grid(cairo_t* cr, int x, int y, int width, int height, int angle);

/**
 * Draw image.
 * @param[in] cr cairo paint context
 * @param[in] image cairo image surface
 * @param[in] x left offset
 * @param[in] y top offset
 * @param[in] scale image scale
 * @param[in] angle image angle
 */
void draw_image(cairo_t* cr, cairo_surface_t* image,
                int x, int y,
                double scale, int angle);

/**
 * Draw formatted text.
 * @param[in] cr cairo paint context
 * @param[in] x left offset
 * @param[in] y top offset
 * @param[in] fmt string format
 * @param[in] ... data for format
 */
__attribute__((format (printf, 4, 5)))
void draw_text(cairo_t* cr, int x, int y, const char* fmt, ...);
