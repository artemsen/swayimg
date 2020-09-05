// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stdint.h>
#include <cairo/cairo.h>

#define HEADER_SIZE 8

/**
 * Image loader function.
 * @param[in] file path to the image file
 * @param[in] header header data (HEADER_SIZE bytes)
 * @return image surface or NULL if decode failed
 */
typedef cairo_surface_t* (*load)(const char* file, const uint8_t* header);

/**
 * Load image from file.
 * @param[in] file path to the file to load
 * @return image surface or NULL if file format is not supported
 */
cairo_surface_t* load_image(const char* file);

__attribute__((format (printf, 3, 4)))
void log_error(const char* name, int errcode, const char* fmt, ...);
