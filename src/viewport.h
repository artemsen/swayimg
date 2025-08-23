// SPDX-License-Identifier: MIT
// Viewport: displaying part of an image on the surface of a window.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "config.h"
#include "image.h"

/** Fixed image scale. */
enum vp_scale {
    vp_scale_fit_optimal, ///< Fit to window, but not more than 100%
    vp_scale_fit_window,  ///< Fit to window size
    vp_scale_fit_width,   ///< Fit width to window width
    vp_scale_fit_height,  ///< Fit height to window height
    vp_scale_fill_window, ///< Fill the window
    vp_scale_real_size,   ///< Real image size (100%)
    vp_scale_keep_zoom,   ///< Keep absolute zoom across images
};

/** Fixed viewport position. */
enum vp_position {
    vp_pos_free,
    vp_pos_center,
    vp_pos_top,
    vp_pos_bottom,
    vp_pos_left,
    vp_pos_right,
    vp_pos_tl,
    vp_pos_tr,
    vp_pos_bl,
    vp_pos_br,
};

/** Viewport move direction. */
enum vp_move {
    vp_move_up,
    vp_move_down,
    vp_move_left,
    vp_move_right,
};

/** Animation control. */
enum vp_actl {
    vp_actl_start,
    vp_actl_stop,
};

/** Viewport context. */
struct viewport {
    struct image* image; ///< Currently shown image
    size_t frame;        ///< Index of the currently displayed frame

    enum vp_position def_pos; ///< Default image position
    enum vp_scale def_scale;  ///< Default image scale

    double scale;         ///< Scale factor of the image
    ssize_t x, y;         ///< Image position on the window surface
    size_t width, height; ///< Window size

    argb_t bkg_window; ///< Window background mode/color
    argb_t bkg_transp; ///< Transparent image background mode/color

    bool aa_en;      ///< Enable/disable anti-aliasing mode
    enum aa_mode aa; ///< Anti-aliasing mode

    int animation_fd;           ///< Animation timer
    void (*animation_cb)(void); ///< Frame switch handler
};

/**
 * Initialize viewport.
 * @param vp viewport context
 * @param section config section
 */
void viewport_init(struct viewport* vp, const struct config* section);

/**
 * Reset viewport state.
 * @param vp viewport context
 * @param img image to attach
 */
void viewport_reset(struct viewport* vp, struct image* img);

/**
 * Window resize handler.
 * @param vp viewport context
 * @param width,height new window size
 */
void viewport_resize(struct viewport* vp, size_t width, size_t height);

/**
 * Switch to the next/previous frame.
 * @param forward switch direction (next/previous)
 */
void viewport_frame(struct viewport* vp, bool forward);

/**
 * Move viewport.
 * @param vp viewport context
 * @param dir move direction
 * @param px step size in pixels
 */
void viewport_move(struct viewport* vp, enum vp_move dir, size_t px);

/**
 * Rotate viewport on 90 degrees.
 * @param vp viewport context
 */
void viewport_rotate(struct viewport* vp);

/**
 * Set default and current scale mode.
 * @param vp viewport context
 * @param scale name of the mode
 * @return false if mode name is unknown
 */
bool viewport_scale_def(struct viewport* vp, const char* scale);

/**
 * Switch default and current scale mode to the next one.
 * @param vp viewport context
 * @return name of the current scale mode
 */
const char* viewport_scale_switch(struct viewport* vp);

/**
 * Set absolute scale of the image, zooming into / out of the center position.
 * @param vp viewport context
 * @param scale scale factor (1.0 = 100%)
 * @param preserve_x the x position in viewport to zoom into / out of (in pixel)
 * @param preserve_y the y position in viewport to zoom into / out of (in pixel)
 */
void viewport_scale_abs(struct viewport* vp, double scale, size_t preserve_x,
                        size_t preserve_y);

/**
 * Start/stop animation.
 * @param vp viewport context
 * @param op operation to perform
 */
void viewport_anim_ctl(struct viewport* vp, enum vp_actl op);

/**
 * Get current status of animation.
 * @param vp viewport context
 * @return true if animation is currently playing
 */
bool viewport_anim_stat(const struct viewport* vp);

/**
 * Get current frame pixmap.
 * @param vp viewport context
 * @return pointer to current pixmap
 */
const struct pixmap* viewport_pixmap(const struct viewport* vp);

/**
 * Draw image.
 * @param vp viewport context
 * @param wnd pixel map of target window
 */
void viewport_draw(const struct viewport* vp, struct pixmap* wnd);
