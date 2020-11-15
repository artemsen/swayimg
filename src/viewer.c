// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "config.h"
#include "viewer.h"
#include "draw.h"
#include "window.h"
#include "formats/loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <linux/input.h>

// Scale thresholds
#define MIN_SCALE_PIXEL 10
#define MAX_SCALE_TIMES 100

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

/** File list. */
struct file_list {
    const char** files;
    int total;
    int current;
};
static struct file_list file_list;

/** Currently displayed image. */
struct image {
    const char* format;
    cairo_surface_t* surface;
    double scale;
    int x;
    int y;
};
static struct image image;

/** Viewer parameters. */
struct viewer viewer;

/**
 * Move viewport.
 * @param[in] op move operation
 * @return true if position was changed
 */
static bool change_position(enum move_op op)
{
    const int prev_x = image.x;
    const int prev_y = image.y;
    const int img_w = image.scale * cairo_image_surface_get_width(image.surface);
    const int img_h = image.scale * cairo_image_surface_get_height(image.surface);
    const int wnd_w = (int)get_window_width();
    const int wnd_h = (int)get_window_height();
    const int step_x = wnd_w / 10;
    const int step_y = wnd_h / 10;

    switch (op) {
        case move_center_x:
            image.x = wnd_w / 2 - img_w / 2;
            break;
        case move_center_y:
            image.y = wnd_h / 2 - img_h / 2;
            break;

        case move_left:
            if (image.x <= 0) {
                image.x += step_x;
                if (image.x > 0) {
                    image.x = 0;
                }
            }
            break;
        case move_right:
            if (image.x + img_w >= wnd_w) {
                image.x -= step_x;
                if (image.x + img_w < wnd_w) {
                    image.x = wnd_w - img_w;
                }
            }
            break;
        case move_up:
            if (image.y <= 0) {
                image.y += step_y;
                if (image.y > 0) {
                    image.y = 0;
                }
            }
            break;
        case move_down:
            if (image.y + img_h >= wnd_h) {
                image.y -= step_y;
                if (image.y + img_h < wnd_h) {
                    image.y = wnd_h - img_h;
                }
            }
            break;
    }

    return image.x != prev_x || image.y != prev_y;
}

/**
 * Change scale.
 * @param[in] op scale operation
 * @return true if zoom or position were changed
 */
static bool change_scale(enum scale_op op)
{
    bool changed;

    const int img_w = cairo_image_surface_get_width(image.surface);
    const int img_h = cairo_image_surface_get_height(image.surface);
    const int wnd_w = (int)get_window_width();
    const int wnd_h = (int)get_window_height();
    const double scale_step = image.scale / 10.0;
    double prev_scale = image.scale;

    switch (op) {
        case actual_size:
            // 100 %
            image.scale = 1.0;
            break;

        case optimal_scale:
            // 100% or less to fit the window
            image.scale = 1.0;
            if (wnd_w < img_w) {
                image.scale = 1.0 / ((double)img_w / wnd_w);
            }
            if (wnd_h < img_h) {
                const double scale = 1.0f / ((double)img_h / wnd_h);
                if (image.scale > scale) {
                    image.scale = scale;
                }
            }
            break;

        case zoom_in:
            image.scale += scale_step;
            if (image.scale > MAX_SCALE_TIMES) {
                image.scale = MAX_SCALE_TIMES;
            }
            break;

        case zoom_out:
            image.scale -= scale_step;
            if (image.scale * img_w < MIN_SCALE_PIXEL || image.scale * img_h < MIN_SCALE_PIXEL) {
                image.scale = prev_scale; // don't change
            }
            break;
    }

    changed = image.scale != prev_scale;

    // update image position
    if (op == actual_size || op == optimal_scale) {
        changed |= change_position(move_center_x);
        changed |= change_position(move_center_y);
    } else {
        const int prev_w = prev_scale * img_w;
        const int prev_h = prev_scale * img_h;
        const int curr_w = image.scale * img_w;
        const int curr_h = image.scale * img_h;
        if (curr_w < wnd_w) {
            // fits into window width
            changed |= change_position(move_center_x);
        } else {
            // move to save the center of previous image
            const int delta_w = prev_w - curr_w;
            const int cntr_x = wnd_w / 2 - image.x;
            image.x += ((double)cntr_x / prev_w) * delta_w;
            if (image.x > 0) {
                image.x = 0;
            }
        }
        if (curr_h < wnd_h) {
            // fits into window height
            changed |= change_position(move_center_y);
        } else {
            // move to save the center of previous image
            const int delta_h = prev_h - curr_h;
            const int cntr_y = wnd_h / 2 - image.y;
            image.y += ((double)cntr_y / prev_h) * delta_h;
            if (image.y > 0) {
                image.y = 0;
            }
        }
    }

    return changed;
}

/**
 * Load image form specified file.
 * @param[in] file path to the file to load
 * @return true if file was loaded
 */
static bool load_file(const char* file)
{
    cairo_surface_t* img;
    const char* format;

    if (!load_image(file, &img, &format)) {
        return false;
    }

    if (image.surface) {
        cairo_surface_destroy(image.surface);
    }
    image.surface = img;
    image.format = format;
    image.scale = 0.0;
    image.x = 0;
    image.y = 0;

    // setup initial scale and position of the image
    if (viewer.scale > 0 && viewer.scale <= MAX_SCALE_TIMES * 100) {
        image.scale = (double)(viewer.scale) / 100.0;
        change_position(move_center_x);
        change_position(move_center_y);
    } else {
        change_scale(optimal_scale);
    }

    // change window title
    char* title = malloc(strlen(APP_NAME) + strlen(file) + 4);
    if (title) {
        strcpy(title, APP_NAME);
        strcat(title, ": ");
        strcat(title, file);
        set_window_title(title);
        free(title);
    }

    return true;
}

/**
 * Open next file.
 * @param[in] forward move direction (true=forward/false=backward).
 * @return false if no file can be opened
 */
static bool load_next_file(bool forward)
{
    const int delta = forward ? 1 : -1;
    int idx = file_list.current;
    idx += delta;
    while (idx != file_list.current) {
        if (idx >= file_list.total) {
            if (file_list.current < 0) {
                return false; // no one valid file
            }
            idx = 0;
        } else if (idx < 0) {
            idx = file_list.total - 1;
        }
        if (load_file(file_list.files[idx])) {
            file_list.current = idx;
            return true;
        }
        idx += delta;
    }
    return false;
}

/** Draw handler, see handlers::on_redraw */
static void on_redraw(cairo_surface_t* window)
{
    const int img_w = cairo_image_surface_get_width(image.surface);
    const int img_h = cairo_image_surface_get_height(image.surface);
    cairo_t* cr = cairo_create(window);

    // clear canvas
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);

    // image with background
    if (cairo_image_surface_get_format(image.surface) == CAIRO_FORMAT_ARGB32) {
        draw_background(cr, image.x, image.y, image.scale * img_w, image.scale * img_h);
    }
    draw_image(cr, image.surface, image.x, image.y, image.scale);

    // image info: file name, format, size, ...
    if (viewer.show_info) {
        draw_text(cr, 10, 10, "File:   %s\n"
                              "Format: %s\n"
                              "Size:   %ix%i\n"
                              "Scale:  %i%%",
                              file_list.files[file_list.current], image.format,
                              img_w, img_h, (int)(image.scale * 100));
    }

    cairo_destroy(cr);
}

/** Window resize handler, see handlers::on_resize */
static void on_resize(void)
{
    change_scale(optimal_scale);
}

/** Keyboard handler, see handlers::on_keyboard. */
static bool on_keyboard(uint32_t key)
{
    switch (key) {
        case KEY_PAGEUP:
        case KEY_KP9:
            return load_next_file(false);
        case KEY_PAGEDOWN:
        case KEY_KP3:
            return load_next_file(true);
        case KEY_LEFT:
        case KEY_KP4:
        case KEY_H:
            return change_position(move_left);
        case KEY_RIGHT:
        case KEY_KP6:
        case KEY_L:
            return change_position(move_right);
        case KEY_UP:
        case KEY_KP8:
        case KEY_K:
            return change_position(move_up);
        case KEY_DOWN:
        case KEY_KP2:
        case KEY_J:
            return change_position(move_down);
        case KEY_EQUAL:
        case KEY_KPPLUS:
            return change_scale(zoom_in);
        case KEY_MINUS:
        case KEY_KPMINUS:
            return change_scale(zoom_out);
        case KEY_0:
            return change_scale(actual_size);
        case KEY_BACKSPACE:
            return change_scale(optimal_scale);
        case KEY_I:
            viewer.show_info = !viewer.show_info;
            return true;
        case KEY_F11:
        case KEY_F:
            viewer.fullscreen = !viewer.fullscreen;
            enable_fullscreen(viewer.fullscreen);
            return false;
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
            return false;
    }
    return false;
}

bool show_image(const char** files, size_t files_num)
{
    bool rc = false;

    const struct handlers handlers = {
        .on_redraw = on_redraw,
        .on_resize = on_resize,
        .on_keyboard = on_keyboard
    };

    file_list.files = files;
    file_list.total = files_num;
    file_list.current = -1;

    // create unique application id
    char app_id[64];
    struct timeval tv;
    gettimeofday(&tv, NULL);
    snprintf(app_id, sizeof(app_id), APP_NAME "_%lx", tv.tv_sec << 32 | tv.tv_usec);

    // setup window position via Sway IPC
    const int ipc = sway_connect();
    if (ipc != -1) {
        bool fullscreen = false;
        if (!viewer.wnd.width) {
            // get currently focused window state
            sway_current(ipc, &viewer.wnd, &fullscreen);
        }
        if (!fullscreen && viewer.wnd.width) {
            sway_add_rules(ipc, app_id, viewer.wnd.x, viewer.wnd.y);
        }
        sway_disconnect(ipc);
    }

    // create and show GUI window
    rc = create_window(&handlers, viewer.wnd.width, viewer.wnd.height, app_id) &&
         load_next_file(true);
    if (rc) {
        show_window();
    }

    // clean
    destroy_window();
    if (image.surface) {
        cairo_surface_destroy(image.surface);
    }

    return rc;
}
