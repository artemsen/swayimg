// SPDX-License-Identifier: MIT
// Scaling pixmaps.
// Copyright (C) 2024 Abe Wieland <abe.wieland@gmail.com>

#pragma once

#include "pixmap.h"

/** Scale filters. */
enum pixmap_scale {
    pixmap_nearest, ///< Nearest neighbor on up- and downscale
    pixmap_box, ///< Nearest neighbor on upscale, average in a box on downscale
    pixmap_bilinear, ///< Bilinear scaling
    pixmap_bicubic,  ///< Bicubic scaling with the Catmull-Rom spline
    pixmap_mks13,    ///< Magic Kernel with 2013 Sharp approximation
};

/**
 * Draw scaled pixmap.
 * @param scaler scale filter to use
 * @param src source pixmap
 * @param dst destination pixmap
 * @param x,y destination left top coordinates
 * @param scale scale of source pixmap
 * @param alpha flag to use alpha blending
 * Note that this function assumes -(src->width * scale) <= x < dst->width and
 * -(src->height * scale) <= y < dst->height (i.e. that at least some part of
 * the scaled image will appear on the destination)
 */
void pixmap_scale(enum pixmap_scale scaler, const struct pixmap* src,
                  struct pixmap* dst, ssize_t x, ssize_t y, float scale,
                  bool alpha);

/**
 * Return scale method based on name.
 * @param name name to look for
 * @return scale method, or -1 if not found
 */
ssize_t pixmap_scale_index(const char* name);
