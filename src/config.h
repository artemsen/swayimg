// SPDX-License-Identifier: MIT
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "canvas.h"

/** App configuration. */
typedef struct {
    scale_t scale;       ///< Initial scale
    rect_t window;       ///< Window geometry
    uint32_t background; ///< Background color
    bool fullscreen;     ///< Full screen mode
    bool show_info;      ///< Show image info
} config_t;

extern config_t config;

/** Load configuration from file. */
void load_config(void);

/**
 * Set initial scale from one of predefined string (default, fit or real).
 * @param[in] value argument to parse
 * @return false if value format is invalid
 */
bool set_scale(const char* value);

/**
 * Set background type/color from RGB hex string.
 * @param[in] value argument to parse
 * @return false if value format is invalid
 */
bool set_background(const char* value);

/**
 * Set window geometry (position and size) from string "x,y,width,height".
 * @param[in] value argument to parse
 * @return false if value format is invalid
 */
bool set_geometry(const char* value);
