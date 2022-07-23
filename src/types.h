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

/** ARGB color. */
typedef uint32_t argb_t;

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
