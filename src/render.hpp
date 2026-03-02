// SPDX-License-Identifier: MIT
// Multithreaded software renderer of raster images.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "pixmap.hpp"
#include "threadpool.hpp"

class Render {
public:
    /**
     * Get global instance of the render.
     * @return render instance
     */
    static Render& self();

    /**
     * Put one scaled pixmap on another (ARGB or RGB underlays).
     * @param dst destination pixmap
     * @param src source pixmap
     * @param pos left top coordinates of source pixmap on destination
     * @param scale scale of source pixmap
     */
    void draw(Pixmap& dst, const Pixmap& src, const Point& pos,
              const double scale);

    /**
     * Fill the entire pixmap except specified area.
     * @param pm target pixmap
     * @param rect target rectangle area
     * @param color color to set
     */
    void fill_inverse(Pixmap& pm, const Rectangle& rect, const argb_t& color);

    /**
     * Extend image to fill entire pixmap (zoom to fill and blur).
     * @param pm target pixmap
     * @param preserve image area to extend
     */
    void extend_background(Pixmap& pm, const Rectangle& preserve);

    /**
     * Extend image to fill entire pixmap (mirror and blur).
     * @param pm target pixmap
     * @param preserve image area to mirror
     */
    void mirror_background(Pixmap& pm, const Rectangle& preserve);

public:
    bool antialiasing = false; ///< Flag to use anti-aliasing

private:
    ThreadPool tpool; ///< Thread pool used for multithreaded rendering
};
