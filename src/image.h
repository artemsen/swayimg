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

/**
 * Output error information.
 * @param[in] name format name
 * @param[in] errcode system error code
 * @param[in] fmt string format
 * @param[in] ... data for format
 */
__attribute__((format (printf, 3, 4)))
void log_error(const char* name, int errcode, const char* fmt, ...);

// meta data keys
extern const cairo_user_data_key_t meta_fmt_name;
