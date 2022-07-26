// SPDX-License-Identifier: MIT
// Text renderer.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "config.h"

/** Font context. */
struct font;

/**
 * Initialize font.
 * @param ctx font context
 * @param cfg configuration instance
 */
struct font* font_init(struct config* cfg);

/**
 * Free font resources.
 * @param ctx font context
 */
void font_free(struct font* ctx);

/**
 * Get single line height for current font.
 * @param ctx font context
 * @return line height in pixels
 */
size_t font_height(const struct font* ctx);

/**
 * Get width of the line.
 * @param ctx font context
 * @param text string to print
 * @param len length of the input string, 0 for auto
 * @return width id the text in pixels
 */
size_t font_text_width(struct font* ctx, const char* text, size_t len);

/**
 * Print single line on window buffer.
 * @param ctx font context
 * @param wnd_buf window buffer
 * @param wnd_size window buffer size
 * @param pos top-left coordinates of text
 * @param text string to print
 * @param len length of the input string, 0 for auto
 * @return width of the line in pixels
 */
size_t font_print(struct font* ctx, argb_t* wnd_buf,
                  const struct size* wnd_size, const struct point* pos,
                  const char* text, size_t len);
