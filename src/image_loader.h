// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <cairo/cairo.h>

/** Image loader description. */
struct loader {
    /** Name of the image format. */
    const char* format;

    /**
     * Image loader function.
     * @param[in] file path to the image file
     * @param[in] header header data
     * @param[in] header_len length if header data in bytes
     * @return image surface or NULL if decode failed
     */
    cairo_surface_t* (*load)(const char* file, const uint8_t* header, size_t header_len);
};

/**
 * Output error information.
 * @param[in] name format name
 * @param[in] errcode system error code
 * @param[in] fmt string format
 * @param[in] ... data for format
 */
__attribute__((format (printf, 3, 4)))
void load_error(const char* name, int errcode, const char* fmt, ...);
