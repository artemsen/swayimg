// SPDX-License-Identifier: MIT
// Common types and constants.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/** ARGB color. */
typedef uint32_t argb_t;

// shifts for each channel in argb_t
#define ARGB_A_SHIFT 24
#define ARGB_R_SHIFT 16
#define ARGB_G_SHIFT 8
#define ARGB_B_SHIFT 0

// get channel value from argb_t
#define ARGB_GET_A(c) (((c) >> ARGB_A_SHIFT) & 0xff)
#define ARGB_GET_R(c) (((c) >> ARGB_R_SHIFT) & 0xff)
#define ARGB_GET_G(c) (((c) >> ARGB_G_SHIFT) & 0xff)
#define ARGB_GET_B(c) (((c) >> ARGB_B_SHIFT) & 0xff)

// create argb_t from channel value
#define ARGB_SET_A(a) (((a)&0xff) << ARGB_A_SHIFT)
#define ARGB_SET_R(r) (((r)&0xff) << ARGB_R_SHIFT)
#define ARGB_SET_G(g) (((g)&0xff) << ARGB_G_SHIFT)
#define ARGB_SET_B(b) (((b)&0xff) << ARGB_B_SHIFT)

// convert RGBA to ARGB
#define ARGB_SET_ABGR(c)                             \
    ((c & 0xff00ff00) | ARGB_SET_R(ARGB_GET_B(c)) | \
     ARGB_SET_B(ARGB_GET_R(c)))

// alpha blending (a=alpha, s=target alpha, b=background, f=foreground)
#define ARGB_ALPHA_BLEND(a, s, b, f)                                          \
    ARGB_SET_A(s) |                                                          \
        ARGB_SET_R((a * ARGB_GET_R(f) + (256 - a) * ARGB_GET_R(b)) >> 8) | \
        ARGB_SET_G((a * ARGB_GET_G(f) + (256 - a) * ARGB_GET_G(b)) >> 8) | \
        ARGB_SET_B((a * ARGB_GET_B(f) + (256 - a) * ARGB_GET_B(b)) >> 8)

/** 2D coordinates. */
struct point {
    ssize_t x;
    ssize_t y;
};

/** Size description. */
struct size {
    size_t width;
    size_t height;
};

/** Rectangle description. */
struct rect {
    ssize_t x;
    ssize_t y;
    size_t width;
    size_t height;
};
