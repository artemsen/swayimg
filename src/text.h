// SPDX-License-Identifier: MIT
// Text renderer.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "font.h"
#include "pixmap.h"

/** Text positions. */
enum text_position {
    text_center,
    text_top_left,
    text_top_right,
    text_bottom_left,
    text_bottom_right,
};

/** Key/value line. */
struct text_keyval {
    struct text_surface key;
    struct text_surface value;
};

/**
 * Initialize text canvas context.
 */
void text_create(void);

/**
 * Print text line.
 * @param wnd destination window
 * @param pos text position
 * @param text text surface to draw
 */
void text_print(struct pixmap* wnd, enum text_position pos,
                const struct text_surface* text);

/**
 * Print block of text (array of lines).
 * @param wnd destination window
 * @param pos block position
 * @param lines array of lines to print
 * @param lines_num total number of lines
 */
void text_print_lines(struct pixmap* wnd, enum text_position pos,
                      const struct text_surface* lines, size_t lines_num);

/**
 * Print block of key/value text.
 * @param wnd destination window
 * @param pos block position
 * @param lines array of key/value lines to print
 * @param lines_num total number of lines
 */
void text_print_keyval(struct pixmap* wnd, enum text_position pos,
                       const struct text_keyval* lines, size_t lines_num);
