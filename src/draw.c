// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "draw.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Text render parameters
#define FONT_FAMILY "monospace"
#define FONT_SIZE 16
#define LINE_SPACING 2
#define TEXT_COLOR 0xb2b2b2
#define TEXT_SHADOW 0x101010

// Background grid parameters
#define GRID_STEP 10
#define GRID_COLOR1 0x333333
#define GRID_COLOR2 0x4c4c4c

// Convert color components from bytes (hex rgb) to doubles
#define RED(c) ((double)((c >> 16) & 0xff) / 255.0)
#define GREEN(c) ((double)((c >> 8) & 0xff) / 255.0)
#define BLUE(c) ((double)(c & 0xff) / 255.0)

void draw_grid(cairo_t* cr, int x, int y, int width, int height, int angle)
{
    cairo_translate(cr, x, y);

    // rotate
    if (angle == 90 || angle == 270) {
        cairo_translate(cr, width / 2, height / 2);
        cairo_rotate(cr, angle * 3.14159 / 180);
        cairo_translate(cr, -width / 2, -height / 2);
    }

    // fill with the first color
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgb(cr, RED(GRID_COLOR1), GREEN(GRID_COLOR1),
                         BLUE(GRID_COLOR1));
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    // draw lighter cells with the second color
    cairo_set_source_rgb(cr, RED(GRID_COLOR2), GREEN(GRID_COLOR2),
                         BLUE(GRID_COLOR2));
    for (y = 0; y < height; y += GRID_STEP) {
        const int cell_height = y + GRID_STEP < height ? GRID_STEP : height - y;
        int cell_x = y / GRID_STEP % 2 ? 0 : GRID_STEP;
        for (x = 0; cell_x < width; cell_x += 2 * GRID_STEP) {
            const int cell_width =
                cell_x + GRID_STEP < width ? GRID_STEP : width - cell_x;
            cairo_rectangle(cr, cell_x, y, cell_width, cell_height);
            cairo_fill(cr);
        }
    }

    cairo_identity_matrix(cr);
}

void draw_image(cairo_t* cr, cairo_surface_t* image, int x, int y, double scale,
                int angle)
{
    cairo_translate(cr, x, y);
    cairo_scale(cr, scale, scale);

    const int width = cairo_image_surface_get_width(image);
    const int height = cairo_image_surface_get_height(image);
    cairo_translate(cr, width / 2, height / 2);
    cairo_rotate(cr, angle * 3.14159 / 180);
    cairo_translate(cr, -width / 2, -height / 2);

    cairo_set_source_surface(cr, image, 0, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_paint(cr);

    cairo_identity_matrix(cr);
}

void draw_text(cairo_t* cr, int x, int y, const char* fmt, ...)
{
    va_list args;
    char* buf;
    int len;

    va_start(args, fmt);
    len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (len <= 0) {
        return;
    }
    buf = malloc(len + 1 /* last null */);
    if (!buf) {
        return;
    }

    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    cairo_select_font_face(cr, FONT_FAMILY, CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, FONT_SIZE);

    // print line by line
    char* start = buf;
    while (1) {
        char* end = strchr(start, '\n');
        if (end) {
            *end = 0;
        }

        // shadow
        cairo_set_source_rgb(cr, RED(TEXT_SHADOW), GREEN(TEXT_SHADOW),
                             BLUE(TEXT_SHADOW));
        cairo_move_to(cr, x + 1, y + 1 + FONT_SIZE);
        cairo_show_text(cr, start);
        // normal text
        cairo_set_source_rgb(cr, RED(TEXT_COLOR), GREEN(TEXT_COLOR),
                             BLUE(TEXT_COLOR));
        cairo_move_to(cr, x, y + FONT_SIZE);
        cairo_show_text(cr, start);

        if (!end) {
            break;
        }
        start = end + 1;
        y += FONT_SIZE + LINE_SPACING;
    }

    free(buf);
}
