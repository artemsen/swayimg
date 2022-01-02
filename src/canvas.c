// SPDX-License-Identifier: MIT
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "canvas.h"

#include "window.h"

#include <stdio.h>
#include <stdlib.h>

// Text render parameters
#define FONT_FAMILY  "monospace"
#define FONT_SIZE    16
#define LINE_SPACING 2
#define TEXT_COLOR   0xb2b2b2
#define TEXT_SHADOW  0x101010

// Background grid parameters
#define GRID_STEP   10
#define GRID_COLOR1 0x333333
#define GRID_COLOR2 0x4c4c4c

// Convert color components from bytes (hex rgb) to doubles
#define RED(c)   ((double)((c >> 16) & 0xff) / 255.0)
#define GREEN(c) ((double)((c >> 8) & 0xff) / 255.0)
#define BLUE(c)  ((double)(c & 0xff) / 255.0)

// Scale thresholds
#define MIN_SCALE 10    // pixels
#define MAX_SCALE 100.0 // factor

#define ROTATE_RAD(r) ((r * 90) * 3.14159 / 180)

void reset_canvas(canvas_t* canvas)
{
    canvas->scale = 0.0;
    canvas->rotate = rotate_0;
    canvas->flip = flip_none;
    canvas->x = 0;
    canvas->y = 0;
}

void draw_image(const canvas_t* canvas, cairo_surface_t* image, cairo_t* cairo)
{
    const int width = cairo_image_surface_get_width(image);
    const int height = cairo_image_surface_get_height(image);
    const int center_x = width / 2;
    const int center_y = height / 2;

    cairo_matrix_t matrix;
    cairo_get_matrix(cairo, &matrix);

    cairo_matrix_translate(&matrix, canvas->x, canvas->y);

    // apply scale
    cairo_matrix_scale(&matrix, canvas->scale, canvas->scale);

    // apply flip
    if (canvas->flip) {
        cairo_matrix_translate(&matrix, center_x, center_y);
        if (canvas->flip & flip_vertical) {
            matrix.yy = -matrix.yy;
        }
        if (canvas->flip & flip_horizontal) {
            matrix.xx = -matrix.xx;
        }
        cairo_matrix_translate(&matrix, -center_x, -center_y);
    }

    // apply rotate
    if (canvas->rotate) {
        cairo_matrix_translate(&matrix, center_x, center_y);
        cairo_matrix_rotate(&matrix, ROTATE_RAD(canvas->rotate));
        cairo_matrix_translate(&matrix, -center_x, -center_y);
    }

    // draw
    cairo_set_matrix(cairo, &matrix);
    cairo_set_source_surface(cairo, image, 0, 0);
    cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
    cairo_paint(cairo);

    cairo_identity_matrix(cairo);
}

void draw_background(const canvas_t* canvas, cairo_surface_t* image,
                     cairo_t* cairo)
{
    const int width = canvas->scale * cairo_image_surface_get_width(image);
    const int height = canvas->scale * cairo_image_surface_get_height(image);

    cairo_translate(cairo, canvas->x, canvas->y);

    // rotate
    if (canvas->rotate == rotate_90 || canvas->rotate == rotate_270) {
        const int center_x = width / 2;
        const int center_y = height / 2;
        cairo_translate(cairo, center_x, center_y);
        cairo_rotate(cairo, ROTATE_RAD(canvas->rotate));
        cairo_translate(cairo, -center_x, -center_y);
    }

    // fill with the first color
    cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgb(cairo, RED(GRID_COLOR1), GREEN(GRID_COLOR1),
                         BLUE(GRID_COLOR1));
    cairo_rectangle(cairo, 0, 0, width, height);
    cairo_fill(cairo);

    // draw lighter cells with the second color
    cairo_set_source_rgb(cairo, RED(GRID_COLOR2), GREEN(GRID_COLOR2),
                         BLUE(GRID_COLOR2));
    for (int y = 0; y < height; y += GRID_STEP) {
        const int cell_height = y + GRID_STEP < height ? GRID_STEP : height - y;
        int cell_x = y / GRID_STEP % 2 ? 0 : GRID_STEP;
        for (; cell_x < width; cell_x += 2 * GRID_STEP) {
            const int cell_width =
                cell_x + GRID_STEP < width ? GRID_STEP : width - cell_x;
            cairo_rectangle(cairo, cell_x, y, cell_width, cell_height);
            cairo_fill(cairo);
        }
    }

    cairo_identity_matrix(cairo);
}

void draw_text(cairo_t* cairo, int x, int y, const char* text)
{
    // set font
    cairo_font_face_t* font = cairo_toy_font_face_create(
        FONT_FAMILY, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_face(cairo, font);
    cairo_set_font_size(cairo, FONT_SIZE);

    // shadow
    cairo_set_source_rgb(cairo, RED(TEXT_SHADOW), GREEN(TEXT_SHADOW),
                         BLUE(TEXT_SHADOW));
    cairo_move_to(cairo, x + 1, y + 1 + FONT_SIZE);
    cairo_show_text(cairo, text);

    // normal text
    cairo_set_source_rgb(cairo, RED(TEXT_COLOR), GREEN(TEXT_COLOR),
                         BLUE(TEXT_COLOR));
    cairo_move_to(cairo, x, y + FONT_SIZE);
    cairo_show_text(cairo, text);

    cairo_set_font_face(cairo, NULL);
    cairo_font_face_destroy(font);
}

void draw_lines(cairo_t* cairo, int x, int y, const char** lines)
{
    while (*lines) {
        draw_text(cairo, x, y, *lines);
        ++lines;
        y += FONT_SIZE + LINE_SPACING;
    }
}

bool move_viewpoint(canvas_t* canvas, cairo_surface_t* image, move_t direction)
{
    const int img_w = canvas->scale * cairo_image_surface_get_width(image);
    const int img_h = canvas->scale * cairo_image_surface_get_height(image);
    const int wnd_w = (int)get_window_width();
    const int wnd_h = (int)get_window_height();
    const int step_x = wnd_w / 10;
    const int step_y = wnd_h / 10;

    int x = canvas->x;
    int y = canvas->y;

    switch (direction) {
        case center_vertical:
            y = wnd_h / 2 - img_h / 2;
            break;
        case center_horizontal:
            x = wnd_w / 2 - img_w / 2;
            break;
        case step_left:
            if (x <= 0) {
                x += step_x;
                if (x > 0) {
                    x = 0;
                }
            }
            break;
        case step_right:
            if (x + img_w >= wnd_w) {
                x -= step_x;
                if (x + img_w < wnd_w) {
                    x = wnd_w - img_w;
                }
            }
            break;
        case step_up:
            if (y <= 0) {
                y += step_y;
                if (y > 0) {
                    y = 0;
                }
            }
            break;
        case step_down:
            if (y + img_h >= wnd_h) {
                y -= step_y;
                if (y + img_h < wnd_h) {
                    y = wnd_h - img_h;
                }
            }
            break;
    }

    if (canvas->x != x || canvas->y != y) {
        canvas->x = x;
        canvas->y = y;
        return true;
    }

    return false;
}

/**
 * Move view point by scale delta considering window center.
 * @param[in] canvas parameters of the image
 * @param[in] image image surface
 * @param[in] delta scale delta
 */
static void move_scaled(canvas_t* canvas, cairo_surface_t* image, double delta)
{
    const int img_w = cairo_image_surface_get_width(image);
    const int img_h = cairo_image_surface_get_height(image);
    const int wnd_w = (int)get_window_width();
    const int wnd_h = (int)get_window_height();
    const int old_w = (canvas->scale - delta) * img_w;
    const int old_h = (canvas->scale - delta) * img_h;
    const int new_w = canvas->scale * img_w;
    const int new_h = canvas->scale * img_h;

    if (new_w < wnd_w) {
        // fits into window width
        move_viewpoint(canvas, image, center_horizontal);
    } else {
        // move to save the center of previous coordinates
        const int delta_w = old_w - new_w;
        const int cntr_x = wnd_w / 2 - canvas->x;
        const int delta_x = ((double)cntr_x / old_w) * delta_w;
        if (delta_x) {
            canvas->x += delta_x;
            if (canvas->x > 0) {
                canvas->x = 0;
            }
        }
    }

    if (new_h < wnd_h) {
        //  fits into window height
        move_viewpoint(canvas, image, center_vertical);
    } else {
        // move to save the center of previous coordinates
        const int delta_h = old_h - new_h;
        const int cntr_y = wnd_h / 2 - canvas->y;
        const int delta_y = ((double)cntr_y / old_h) * delta_h;
        if (delta_y) {
            canvas->y += delta_y;
            if (canvas->y > 0) {
                canvas->y = 0;
            }
        }
    }
}

bool apply_scale(canvas_t* canvas, cairo_surface_t* image, scale_t op)
{
    const int img_w = cairo_image_surface_get_width(image);
    const int img_h = cairo_image_surface_get_height(image);
    const int wnd_w = get_window_width();
    const int wnd_h = get_window_height();
    const bool swap =
        (canvas->rotate == rotate_90 || canvas->rotate == rotate_270);
    const double max_w = swap ? img_h : img_w;
    const double max_h = swap ? img_w : img_h;
    const double step = canvas->scale / 10.0;

    double scale = canvas->scale;

    switch (op) {
        case scale_fit_or100: {
            const double scale_w = 1.0 / (max_w / wnd_w);
            const double scale_h = 1.0 / (max_h / wnd_h);
            scale = scale_w < scale_h ? scale_w : scale_h;
            if (scale > 1.0) {
                scale = 1.0;
            }
            break;
        }
        case scale_fit_window: {
            const double scale_w = 1.0 / (max_w / wnd_w);
            const double scale_h = 1.0 / (max_h / wnd_h);
            scale = scale_h < scale_w ? scale_h : scale_w;
            break;
        }
        case scale_100:
            scale = 1.0; // 100 %
            break;
        case zoom_in:
            if (canvas->scale < MAX_SCALE) {
                scale = canvas->scale + step;
                if (scale > MAX_SCALE) {
                    scale = MAX_SCALE;
                }
            }
            break;
        case zoom_out:
            scale -= step;
            if (scale * img_w < MIN_SCALE && scale * img_h < MIN_SCALE) {
                scale = canvas->scale; // don't change
            }
            break;
    }

    if (canvas->scale != scale) {
        // move viewpoint
        const double delta = scale - canvas->scale;
        canvas->scale = scale;
        if (op == scale_fit_window || op == scale_fit_or100 ||
            op == scale_100) {
            move_viewpoint(canvas, image, center_vertical);
            move_viewpoint(canvas, image, center_horizontal);
        } else {
            move_scaled(canvas, image, delta);
        }
        return true;
    }

    return false;
}

void apply_rotate(canvas_t* canvas, bool clockwise)
{
    if (clockwise) {
        if (canvas->rotate == rotate_270) {
            canvas->rotate = rotate_0;
        } else {
            ++canvas->rotate;
        }
    } else {
        if (canvas->rotate == rotate_0) {
            canvas->rotate = rotate_270;
        } else {
            --canvas->rotate;
        }
    }
}

void apply_flip(canvas_t* canvas, flip_t flip)
{
    canvas->flip ^= flip;
}
