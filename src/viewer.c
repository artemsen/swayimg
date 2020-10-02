// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "config.h"
#include "image.h"
#include "viewer.h"
#include "window.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>
#include <linux/input.h>

// text render parameters
#define FONT_SIZE    16
#define LINE_SPACING 2

/** Scale operation types. */
enum scale_op {
    actual_size,
    optimal_scale,
    zoom_in,
    zoom_out
};

/** Move operation types. */
enum move_op {
    move_center_x,
    move_center_y,
    move_left,
    move_right,
    move_up,
    move_down
};

/** Viewer context. */
struct context {
    cairo_surface_t* img;
    double scale;
    int img_x;
    int img_y;
    int wnd_width;
    int wnd_height;
    bool show_info;
    const char* file;
};
static struct context ctx;

/**
 * Move viewport.
 * @param[in] op move operation
 */
static void change_position(enum move_op op)
{
    const int img_w = ctx.scale * cairo_image_surface_get_width(ctx.img);
    const int img_h = ctx.scale * cairo_image_surface_get_height(ctx.img);
    const int step_x = ctx.wnd_width / 10;
    const int step_y = ctx.wnd_height / 10;

    switch (op) {
        case move_center_x:
            ctx.img_x = ctx.wnd_width / 2 - img_w / 2;
            break;
        case move_center_y:
            ctx.img_y = ctx.wnd_height / 2 - img_h / 2;
            break;

        case move_left:
            if (ctx.img_x <= 0) {
                ctx.img_x += step_x;
                if (ctx.img_x > 0) {
                    ctx.img_x = 0;
                }
            }
            break;
        case move_right:
            if (ctx.img_x + img_w >= ctx.wnd_width) {
                ctx.img_x -= step_x;
                if (ctx.img_x + img_w < ctx.wnd_width) {
                    ctx.img_x = ctx.wnd_width - img_w;
                }
            }
            break;
        case move_up:
            if (ctx.img_y <= 0) {
                ctx.img_y += step_y;
                if (ctx.img_y > 0) {
                    ctx.img_y = 0;
                }
            }
            break;
        case move_down:
            if (ctx.img_y + img_h >= ctx.wnd_height) {
                ctx.img_y -= step_y;
                if (ctx.img_y + img_h < ctx.wnd_height) {
                    ctx.img_y = ctx.wnd_height - img_h;
                }
            }
            break;
    }
}

/**
 * Change scale.
 * @param[in] op scale operation
 */
static void change_scale(enum scale_op op)
{
    const int img_w = cairo_image_surface_get_width(ctx.img);
    const int img_h = cairo_image_surface_get_height(ctx.img);
    const double scale_step = ctx.scale / 10.0;
    double new_scale;

    switch (op) {
        case actual_size:
            // 100 %
            new_scale = 1.0;
            break;

        case optimal_scale: 
            // 100% or less to fit the window
            new_scale = 1.0;
            if (ctx.wnd_width < img_w) {
                new_scale = 1.0 / ((double)img_w / ctx.wnd_width);
            }
            if (ctx.wnd_height < img_h) {
                const double scale = 1.0f / ((double)img_h / ctx.wnd_height);
                if (new_scale > scale) {
                    new_scale = scale;
                }
            }
            break;

        case zoom_in:
            new_scale = ctx.scale + scale_step;
            break;

        case zoom_out:
            new_scale = ctx.scale - scale_step;
            // at least 10 pixel
            if (new_scale * img_w < 10 || new_scale * img_h < 10) {
                new_scale = ctx.scale; // don't change
            }
            break;
    }

    // update image position
    if (op == actual_size || op == optimal_scale) {
        ctx.scale = new_scale;
        change_position(move_center_x);
        change_position(move_center_y);
    } else {
        const int prev_w = ctx.scale * cairo_image_surface_get_width(ctx.img);
        const int prev_h = ctx.scale * cairo_image_surface_get_height(ctx.img);
        ctx.scale = new_scale;
        const int curr_w = ctx.scale * cairo_image_surface_get_width(ctx.img);
        const int curr_h = ctx.scale * cairo_image_surface_get_height(ctx.img);
        if (curr_w < ctx.wnd_width) {
            // fits into window width
            change_position(move_center_x);
        } else {
            // move to save the center of previous image
            const int delta_w = prev_w - curr_w;
            const int cntr_x = ctx.wnd_width / 2 - ctx.img_x;
            ctx.img_x += ((double)cntr_x / prev_w) * delta_w;
            if (ctx.img_x > 0) {
                ctx.img_x = 0;
            }
        }
        if (curr_h < ctx.wnd_height) {
            // fits into window height
            change_position(move_center_y);
        } else {
            // move to save the center of previous image
            const int delta_h = prev_h - curr_h;
            const int cntr_y = ctx.wnd_height / 2 - ctx.img_y;
            ctx.img_y += ((double)cntr_y / prev_h) * delta_h;
            if (ctx.img_y > 0) {
                ctx.img_y = 0;
            }
        }
    }
}

/**
 * Draw chess board as background.
 * @param[in] cr cairo paint context
 */
static void chess_background(cairo_t* cr)
{
    const int step = 10;

    // clip to window size
    cairo_rectangle_int_t board = {
        .x = ctx.img_x,
        .y = ctx.img_y,
        .width = ctx.scale * cairo_image_surface_get_width(ctx.img),
        .height = ctx.scale * cairo_image_surface_get_height(ctx.img)
    };
    if (board.x < 0) {
        board.width += board.x;
        board.x = 0;
    }
    if (board.y < 0) {
        board.height += board.y;
        board.y = 0;
    }
    cairo_surface_t* window = cairo_get_target(cr);
    int end_x = cairo_image_surface_get_width(window);
    int end_y = cairo_image_surface_get_height(window);
    if (ctx.img_x + board.width > end_x) {
        board.width -= end_x - (ctx.img_x + board.width);
    }
    if (ctx.img_y + board.height > end_y) {
        board.height -= end_y - (ctx.img_y + board.height);
    }

    // fill with the first color
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_rectangle(cr, board.x, board.y, board.width, board.height);
    cairo_fill(cr);

    // draw lighter cells
    end_x = board.x + board.width;
    end_y = board.y + board.height;
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    cairo_rectangle_int_t cell;
    for (cell.y = board.y; cell.y < end_y; cell.y += step) {
        cell.height = cell.y + step < end_y ? step : end_y - cell.y;
        cell.x = board.x + (cell.y / step % 2 ? 0 : step);
        for (; cell.x < end_x; cell.x += 2 * step) {
            cell.width = cell.x + step < end_x ? step : end_x - cell.x;
            cairo_rectangle(cr, cell.x, cell.y, cell.width, cell.height);
            cairo_fill(cr);
        }
    }
}

/**
 * Draw formatted text line with shadow.
 * @param[in] cr cairo paint context
 * @param[in] x horizontal coordinate
 * @param[in] y vertical coordinate
 * @param[in] text text to display
 */
static void draw_text(cairo_t* cr, int x, int y, const char* text, ...)
{
    char buf[128];

    va_list args;
    va_start(args, text);
    vsnprintf(buf, sizeof(buf), text, args);
    va_end(args);

    cairo_select_font_face(cr, "monospace",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, FONT_SIZE);
    for (int i = 0; i <= 1; ++i) {
        cairo_set_source_rgb(cr, i * 0.7, i * 0.7, i * 0.7); 
        cairo_move_to(cr, 1 - i, y + 1 - i + FONT_SIZE);
        cairo_show_text(cr, buf);
    }
}

/**
 * Print image information.
 * @param[in] cr cairo paint context
 */
static void print_info(cairo_t* cr)
{
    int y = 0;

    // file name
    const char* name = strrchr(ctx.file, '/');
    if (name) {
        ++name; // skip delimiter
    } else {
        name = ctx.file;
    }
    draw_text(cr, 0, y, "File:   %s", name);

    // image format
    const char* fmt = cairo_surface_get_user_data(ctx.img, &meta_fmt_name);
    if (fmt) {
        y += LINE_SPACING + FONT_SIZE;
        draw_text(cr, 0, y, "Format: %s", fmt);
    }

    // image size
    y += LINE_SPACING + FONT_SIZE;
    draw_text(cr, 0, y, "Size:   %ix%i",
                        cairo_image_surface_get_width(ctx.img),
                        cairo_image_surface_get_height(ctx.img));

    // current scale
    y += LINE_SPACING + FONT_SIZE;
    draw_text(cr, 0, y, "Scale:  %i%%", (int)(ctx.scale * 100));
}

/** Draw handler, see window::on_redraw */
static void redraw(cairo_surface_t* window)
{
    cairo_t* cr = cairo_create(window);

    // clear canvas
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);

    // background
    if (cairo_image_surface_get_format(ctx.img) == CAIRO_FORMAT_ARGB32) {
        chess_background(cr);
    }

    // scale, move and draw image
    cairo_matrix_t matrix;
    cairo_matrix_t translate;
    cairo_matrix_init_translate(&translate, ctx.img_x, ctx.img_y);
    cairo_matrix_init_scale(&matrix, ctx.scale, ctx.scale);
    cairo_matrix_multiply(&matrix, &matrix, &translate);
    cairo_set_matrix(cr, &matrix);
    cairo_set_source_surface(cr, ctx.img, 0, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_paint(cr);
    cairo_identity_matrix(cr);

    // image info: path to the file, format, size, ...
    if (ctx.show_info) {
        print_info(cr);
    }
 
    cairo_destroy(cr);
}

/** Window resize handler, see window::on_resize */
static void resize(cairo_surface_t* window)
{
    ctx.wnd_width = cairo_image_surface_get_width(window);
    ctx.wnd_height = cairo_image_surface_get_height(window);
    change_scale(optimal_scale);
}

/** Keyboard handler, see window::on_keyboard. */
static bool handle_key(uint32_t key)
{
    switch (key) {
        case KEY_LEFT:
        case KEY_KP4:
        case KEY_H:
            change_position(move_left);
            return true;
        case KEY_RIGHT:
        case KEY_KP6:
        case KEY_L:
            change_position(move_right);
            return true;
        case KEY_UP:
        case KEY_KP8:
        case KEY_K:
            change_position(move_up);
            return true;
        case KEY_DOWN:
        case KEY_KP2:
        case KEY_J:
            change_position(move_down);
            return true;
        case KEY_EQUAL:
        case KEY_KPPLUS:
            change_scale(zoom_in);
            return true;
        case KEY_MINUS:
        case KEY_KPMINUS:
            change_scale(zoom_out);
            return true;
        case KEY_0:
            change_scale(actual_size);
            return true;
        case KEY_BACKSPACE:
            change_scale(optimal_scale);
            return true;
        case KEY_I:
            ctx.show_info = !ctx.show_info;
            return true;
        case KEY_ESC:
        case KEY_ENTER:
        case KEY_KPENTER:
        case KEY_F3:
        case KEY_F4:
        case KEY_F10:
        case KEY_Q:
        case KEY_E:
        case KEY_X:
            close_window();
            break;
    }
    return false;
}

/**
 * Allocate and format string.
 * @param[in] fmt string format
 * @param[in] ... arguments
 * @return formatted string, caller must free the string
 */
static char* format_string(const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    const int len = vsnprintf(NULL, 0, fmt, args) + 1 /* last null */;
    va_end(args);

    char* buf = malloc(len);
    if (!buf) {
        fprintf(stderr, "Not enough memory\n");
        return NULL;
    }
    va_start(args, fmt);
    vsnprintf(buf, len, fmt, args);
    va_end(args);

    return buf;
}

bool show_image(const struct viewer* params)
{
    bool rc = false;
    char* app_id = NULL;
    char* title = NULL;

    ctx.img = load_image(params->file);
    if (!ctx.img) {
        goto done;
    }

    title = format_string(APP_NAME ": %s", params->file);
    if (!title) {
        goto done;
    }

    struct window wnd = {
        .redraw = redraw,
        .resize = resize,
        .keyboard = handle_key,
        .width = 0,
        .height = 0,
        .fullscreen = params->fullscreen,
        .app_id = params->app_id ? params->app_id : APP_NAME,
        .title = title
    };

    // setup window position via Sway IPC
    if (!wnd.fullscreen) {
        const int ipc = sway_connect();
        if (ipc != -1) {
            bool rc = true;
            struct rect rect;
            if (params->wnd) {
                rect = *params->wnd;
            } else {
                // get currently focused window state
                rc = sway_current(ipc, &rect, &wnd.fullscreen);
            }
            if (rc) {
                wnd.width = rect.width;
                wnd.height = rect.height;
                if (!wnd.fullscreen) {
                    if (!params->app_id) {
                        // create unique app id
                        struct timeval tv;
                        gettimeofday(&tv, NULL);
                        app_id = format_string(APP_NAME "_%lx", tv.tv_sec << 32 | tv.tv_usec);
                        if (!app_id) {
                            sway_disconnect(ipc);
                            goto done;
                        }
                        wnd.app_id = app_id;
                    }
                    sway_add_rules(ipc, wnd.app_id, rect.x, rect.y);
                }
            }
            sway_disconnect(ipc);
        }
    }

    // normalize window size
    if (!wnd.width) {
        wnd.width = cairo_image_surface_get_width(ctx.img);
    }
    if (!wnd.height) {
        wnd.height = cairo_image_surface_get_height(ctx.img);
    }
    ctx.wnd_width = wnd.width;
    ctx.wnd_height = wnd.height;

    // setup initial scale and position of the image
    if (params->scale <= 0) {
        change_scale(optimal_scale);
    } else {
        ctx.scale = (double)(params->scale) / 100.0;
    }

    ctx.file = params->file;
    ctx.show_info = params->show_info;

    // create and show GUI window
    rc = show_window(&wnd);

done:
    // clean
    if (ctx.img) {
        cairo_surface_destroy(ctx.img);
    }
    if (app_id) {
        free(app_id);
    }
    if (title) {
        free(title);
    }

    return rc;
}
