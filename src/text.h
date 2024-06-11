// SPDX-License-Identifier: MIT
// Text renderer.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "info.h"
#include "pixmap.h"

/**
 * Initialize text canvas context.
 */
void text_init(void);

/**
 * Print information text block.
 * @param wnd destination window
 * @param pos block position
 * @param lines array of lines to print
 * @param lines_num total number of lines
 */
void text_print(struct pixmap* wnd, enum info_position pos,
                const struct info_line* lines, size_t lines_num);

/**
 * Print text block at the center of window.
 * @param wnd destination window
 * @param lines array of lines to print
 * @param lines_num total number of lines
 */
void text_print_centered(struct pixmap* wnd, const struct text_surface* lines,
                         size_t lines_num);
