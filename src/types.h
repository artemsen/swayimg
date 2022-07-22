// SPDX-License-Identifier: MIT
// Common types and constants.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

// Grid background mode id
#define BACKGROUND_GRID UINT32_MAX

/** Rotate angles. */
typedef enum {
    rotate_0,   ///< No rotate
    rotate_90,  ///< 90 degrees, clockwise
    rotate_180, ///< 180 degrees
    rotate_270  ///< 270 degrees, clockwise
} rotate_t;

/** Flags of the flip transformation. */
typedef enum {
    flip_none,
    flip_vertical,
    flip_horizontal,
} flip_t;

/** Scaling operations. */
typedef enum {
    scale_fit_or100,  ///< Fit to window, but not more than 100%
    scale_fit_window, ///< Fit to window size
    scale_100,        ///< Real image size (100%)
    zoom_in,          ///< Enlarge by one step
    zoom_out          ///< Reduce by one step
} scale_t;

/** Direction of viewpoint movement. */
typedef enum {
    center_vertical,   ///< Center vertically
    center_horizontal, ///< Center horizontally
    step_left,         ///< One step to the left
    step_right,        ///< One step to the right
    step_up,           ///< One step up
    step_down          ///< One step down
} move_t;

/** Position on the text block relative to the output window. */
typedef enum {
    text_top_left,
    text_top_right,
    text_bottom_left,
    text_bottom_right
} text_position_t;

/** Rectangle description. */
typedef struct {
    ssize_t x;
    ssize_t y;
    size_t width;
    size_t height;
} rect_t;
