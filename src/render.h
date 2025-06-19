// SPDX-License-Identifier: MIT
// Multithreaded software renderer for raster images.
// Copyright (C) 2024 Abe Wieland <abe.wieland@gmail.com>

#pragma once

#include "config.h"
#include "pixmap.h"

/** Scale filters (anti-aliasing mode). */
enum aa_mode {
    aa_nearest,  ///< Nearest neighbor on up- and downscale
    aa_box,      ///< Nearest neighbor on upscale, average in a box on downscale
    aa_bilinear, ///< Bilinear scaling
    aa_bicubic,  ///< Bicubic scaling with the Catmull-Rom spline
    aa_mks13,    ///< Magic Kernel with 2013 Sharp approximation
};

/**
 * Get anti-aliasing mode from config.
 * @param section config section
 * @param section,key section name and key
 * @return anti-aliasing mode
 */
enum aa_mode aa_init(const struct config* section, const char* key);

/**
 * Switch anti-aliasing mode.
 * @param curr current anti-aliasing mode
 * @param opt switch operation
 * @return new anti-aliasing mode or current if opt has invalid format
 */
enum aa_mode aa_switch(enum aa_mode curr, const char* opt);

/**
 * Get human readable anti-aliasing mode name.
 * @param aa anti-aliasing mode
 * @return anti-aliasing mode name
 */
const char* aa_name(enum aa_mode aa);

/**
 * Render scaled pixmap.
 * @param src source pixmap
 * @param dst destination pixmap
 * @param x,y destination left top coordinates
 * @param scale scale of source pixmap
 * @param scaler scale filter to use (anti-aliasing mode)
 * @param mt flag to use multithreaded rendering
 */
void software_render(const struct pixmap* src, struct pixmap* dst, ssize_t x,
                     ssize_t y, double scale, enum aa_mode scaler, bool mt);
