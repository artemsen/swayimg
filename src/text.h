// SPDX-License-Identifier: MIT
// Text renderer based on the Pango framework.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "config.h"

/** Text renderer context. */
struct text_render;
typedef struct text_render text_render_t;

/** Position on the text block relative to the output window. */
typedef enum {
    text_top_left,
    text_top_right,
    text_bottom_left,
    text_bottom_right
} text_position_t;

/**
 * Initialize text renderer context.
 * @param[in] cfg configuration instance
 * @return created text renderer context
 */
text_render_t* init_text(config_t* cfg);

/**
 * Free text renderer.
 * @param[in] ctx renderer context
 */
void free_text(text_render_t* ctx);

/**
 * Print text.
 * @param[in] ctx renderer context
 * @param[in] cairo output cairo context
 * @param[in] pos text block position
 * @param[in] text text to output
 */
void print_text(text_render_t* ctx, cairo_t* cairo, text_position_t pos,
                const char* text);
