// SPDX-License-Identifier: MIT
// Scaling pixmaps.
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
 * @param cfg config instance
 * @param section,key section name and key
 * @return anti-aliasing mode
 */
enum aa_mode aa_init(const struct config* cfg, const char* section,
                     const char* key);
/**
 * Switch anti-aliasing mode.
 * @param curr current anti-aliasing mode
 * @param opt switch opration
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
 * Draw scaled pixmap.
 * @param scaler scale filter to use
 * @param src source pixmap
 * @param dst destination pixmap
 * @param x,y destination left top coordinates
 * @param scale scale of source pixmap
 * @param alpha flag to use alpha blending
 */
void pixmap_scale(enum aa_mode scaler, const struct pixmap* src,
                  struct pixmap* dst, ssize_t x, ssize_t y, float scale,
                  bool alpha);
