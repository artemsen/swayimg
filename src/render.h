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

struct aa_state {
    enum aa_mode
        when_enabled;  //< Anti-aliasing mode when anti-aliasing is enabled
    enum aa_mode curr; //< Current anti-aliasing mode
};

/**
 * Get anti-aliasing state from config.
 * @param cfg config instance
 * @param section section name
 * @param aa_on_key key name for the anti-aliasing mode when it is on
 * @param aa_start_key key name for whether anti-aliasing should be on by
 * default
 * @return anti-aliasing state
 */
struct aa_state aa_init(const struct config* cfg, const char* section,
                        const char* aa_on_key, const char* aa_start_key);

/**
 * Switch anti-aliasing mode.
 * @param state Current anti-aliasing state to be modified
 * @param opt switch operation
 */
void aa_switch(struct aa_state* curr, const char* opt);

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
