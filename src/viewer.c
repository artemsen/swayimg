// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "config.h"
#include "viewer.h"
#include "draw.h"
#include "window.h"
#include "image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

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

/** Viewer context. */
struct context {
    /** Currently displayed image. */
    struct image* image;
    /** Image scale, 1.0 = 100%. */
    double scale;
    /** Image angle (0/90/180/270). */
    int angle;
    /** Coordinates of the top left corner. */
    int x;
    int y;

    /** File list. */
    struct flist {
        const char** files;
        int total;
        int current;
    } flist;
};
static struct context ctx;

/** Viewer parameters. */
struct viewer viewer;

/**
 * Move viewport.
 * @param[in] op move operation
 * @return true if position was changed
 */
static bool change_position(enum move_op op)
{
    const int prev_x = ctx.x;
    const int prev_y = ctx.y;
    const int img_w = ctx.scale * cairo_image_surface_get_width(ctx.image->surface);
    const int img_h = ctx.scale * cairo_image_surface_get_height(ctx.image->surface);
    const int wnd_w = (int)get_window_width();
    const int wnd_h = (int)get_window_height();
    const int step_x = wnd_w / 10;
    const int step_y = wnd_h / 10;

    switch (op) {
        case move_center_x:
            ctx.x = wnd_w / 2 - img_w / 2;
            break;
        case move_center_y:
            ctx.y = wnd_h / 2 - img_h / 2;
            break;

        case move_left:
            if (ctx.x <= 0) {
                ctx.x += step_x;
                if (ctx.x > 0) {
                    ctx.x = 0;
                }
            }
            break;
        case move_right:
            if (ctx.x + img_w >= wnd_w) {
                ctx.x -= step_x;
                if (ctx.x + img_w < wnd_w) {
                    ctx.x = wnd_w - img_w;
                }
            }
            break;
        case move_up:
            if (ctx.y <= 0) {
                ctx.y += step_y;
                if (ctx.y > 0) {
                    ctx.y = 0;
                }
            }
            break;
        case move_down:
            if (ctx.y + img_h >= wnd_h) {
                ctx.y -= step_y;
                if (ctx.y + img_h < wnd_h) {
                    ctx.y = wnd_h - img_h;
                }
            }
            break;
    }

    return ctx.x != prev_x || ctx.y != prev_y;
}

/**
 * Change scale.
 * @param[in] op scale operation
 * @return true if zoom or position were changed
 */
static bool change_scale(enum scale_op op)
{
    bool changed;

    const int img_w = cairo_image_surface_get_width(ctx.image->surface);
    const int img_h = cairo_image_surface_get_height(ctx.image->surface);
    const int wnd_w = (int)get_window_width();
    const int wnd_h = (int)get_window_height();
    const double scale_step = ctx.scale / 10.0;
    double prev_scale = ctx.scale;

    switch (op) {
        case actual_size:
            // 100 %
            ctx.scale = 1.0;
            break;

        case optimal_scale: {
            // 100% or less to fit the window
            ctx.scale = 1.0;
            const int max_w = ctx.angle == 0 || ctx.angle == 180 ? img_w : img_h;
            const int max_h = ctx.angle == 0 || ctx.angle == 180 ? img_h : img_w;
            if (wnd_w < max_w) {
                ctx.scale = 1.0 / ((double)max_w / wnd_w);
            }
            if (wnd_h < max_h) {
                const double scale = 1.0f / ((double)max_h / wnd_h);
                if (ctx.scale > scale) {
                    ctx.scale = scale;
                }
            }
            break;
            }

        case zoom_in:
            ctx.scale += scale_step;
            if (ctx.scale > MAX_SCALE_TIMES) {
                ctx.scale = MAX_SCALE_TIMES;
            }
            break;

        case zoom_out:
            ctx.scale -= scale_step;
            if (ctx.scale * img_w < MIN_SCALE_PIXEL || ctx.scale * img_h < MIN_SCALE_PIXEL) {
                ctx.scale = prev_scale; // don't change
            }
            break;
    }

    changed = ctx.scale != prev_scale;

    // update image position
    if (op == actual_size || op == optimal_scale) {
        changed |= change_position(move_center_x);
        changed |= change_position(move_center_y);
    } else {
        const int prev_w = prev_scale * img_w;
        const int prev_h = prev_scale * img_h;
        const int curr_w = ctx.scale * img_w;
        const int curr_h = ctx.scale * img_h;
        if (curr_w < wnd_w) {
            // fits into window width
            changed |= change_position(move_center_x);
        } else {
            // move to save the center of previous image
            const int delta_w = prev_w - curr_w;
            const int cntr_x = wnd_w / 2 - ctx.x;
            ctx.x += ((double)cntr_x / prev_w) * delta_w;
            if (ctx.x > 0) {
                ctx.x = 0;
            }
        }
        if (curr_h < wnd_h) {
            // fits into window height
            changed |= change_position(move_center_y);
        } else {
            // move to save the center of previous image
            const int delta_h = prev_h - curr_h;
            const int cntr_y = wnd_h / 2 - ctx.y;
            ctx.y += ((double)cntr_y / prev_h) * delta_h;
            if (ctx.y > 0) {
                ctx.y = 0;
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
    struct image* img = load_image(file);
    if (!img) {
        return false;
    }

    free_image(ctx.image);

    ctx.image = img;
    ctx.scale = 0.0;
    ctx.angle = 0;
    ctx.x = 0;
    ctx.y = 0;

    // setup initial scale and position of the image
    if (viewer.scale > 0 && viewer.scale <= MAX_SCALE_TIMES * 100) {
        ctx.scale = (double)(viewer.scale) / 100.0;
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

static int dirname_length(const char* dir)
{
    int length = strlen(dir);
    while (length >= 0 && dir[length] != '/') {
        length--;
    }
    return length;
}

static const char* get_next_file(bool forward)
{
    if (ctx.flist.total == 0) {
        return NULL;
    }
    ctx.flist.current += forward ? 1 : -1;
    if (ctx.flist.current == ctx.flist.total) {
        ctx.flist.current = 0;
    } else if (ctx.flist.current < 0) {
        ctx.flist.current = ctx.flist.total - 1;
    }
    return ctx.flist.files[ctx.flist.current];
}

static const char* get_next_directory(bool forward)
{
    const int initial = ctx.flist.current;
    const char* current = ctx.flist.files[ctx.flist.current];
    const int current_dir_length = dirname_length(current);
    const char* next = get_next_file(forward);
    int next_dir_length = dirname_length(next);
    while (next && ctx.flist.current != initial) {
        if (current_dir_length != next_dir_length || strncmp(current, next, next_dir_length) != 0) {
            return next;
        }
        next = get_next_file(forward);
        next_dir_length = dirname_length(next);
    }
    return next;
}

/**
 * Open next file.
 * @param[in] forward move direction (true=forward/false=backward).
 * @return false if no file can be opened
 */
static bool load_next_file(bool forward, bool skip_dir)
{
    const char* file = skip_dir ? get_next_directory(forward) : get_next_file(forward);
    if (!file) {
        return false;
    }
    while (file) {
        if(load_file(file)) {
            return true;
        }
        file = get_next_file(forward);
    }
    return false;
}

/** Draw handler, see handlers::on_redraw */
static void on_redraw(cairo_surface_t* window)
{
    const int img_w = cairo_image_surface_get_width(ctx.image->surface);
    const int img_h = cairo_image_surface_get_height(ctx.image->surface);
    cairo_t* cr = cairo_create(window);

    // clear canvas
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);

    // image with background
    if (cairo_image_surface_get_format(ctx.image->surface) == CAIRO_FORMAT_ARGB32) {
        draw_grid(cr, ctx.x, ctx.y, ctx.scale * img_w, ctx.scale * img_h, ctx.angle);
    }
    draw_image(cr, ctx.image->surface, ctx.x, ctx.y, ctx.scale, ctx.angle);

    // image info: file name, format, size, ...
    if (viewer.show_info) {
        draw_text(cr, 10, 10, "File:   %s\n"
                              "Format: %s\n"
                              "Size:   %ix%i\n"
                              "Scale:  %i%%",
                              ctx.flist.files[ctx.flist.current], ctx.image->format,
                              img_w, img_h, (int)(ctx.scale * 100));
    }

    cairo_destroy(cr);
}

/** Window resize handler, see handlers::on_resize */
static void on_resize(void)
{
    change_scale(optimal_scale);
}

/** Keyboard handler, see handlers::on_keyboard. */
static bool on_keyboard(xkb_keysym_t key)
{
    switch (key) {
        case XKB_KEY_SunPageUp:
        case XKB_KEY_p:
            return load_next_file(false, false);
        case XKB_KEY_SunPageDown:
        case XKB_KEY_n:
        case XKB_KEY_space:
            return load_next_file(true, false);
        case XKB_KEY_P:
            return load_next_file(false, true);
        case XKB_KEY_N:
            return load_next_file(true, true);
        case XKB_KEY_Home:
            ctx.flist.current = -1;
            return load_next_file(true, false);
        case XKB_KEY_End:
            ctx.flist.current = 0;
            return load_next_file(false, false);
        case XKB_KEY_Left:
        case XKB_KEY_h:
            return change_position(move_left);
        case XKB_KEY_Right:
        case XKB_KEY_l:
            return change_position(move_right);
        case XKB_KEY_Up:
        case XKB_KEY_k:
            return change_position(move_up);
        case XKB_KEY_Down:
        case XKB_KEY_j:
            return change_position(move_down);
        case XKB_KEY_equal:
        case XKB_KEY_plus:
            return change_scale(zoom_in);
        case XKB_KEY_minus:
            return change_scale(zoom_out);
        case XKB_KEY_0:
            return change_scale(actual_size);
        case XKB_KEY_BackSpace:
            return change_scale(optimal_scale);
        case XKB_KEY_bracketleft:
        case XKB_KEY_bracketright:
            ctx.angle += key == XKB_KEY_bracketleft ? -90 : 90;
            if (ctx.angle < 0) {
                ctx.angle = 270;
            } else if (ctx.angle >= 360) {
                ctx.angle = 0;
            }
            return true;
        case XKB_KEY_i:
            viewer.show_info = !viewer.show_info;
            return true;
        case XKB_KEY_F11:
        case XKB_KEY_f:
            viewer.fullscreen = !viewer.fullscreen;
            enable_fullscreen(viewer.fullscreen);
            return false;
        case XKB_KEY_Escape:
        case XKB_KEY_Return:
        case XKB_KEY_F10:
        case XKB_KEY_q:
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

    ctx.flist.files = files;
    ctx.flist.total = files_num;
    ctx.flist.current = -1;

    // create unique application id
    char app_id[64];
    struct timeval tv;
    gettimeofday(&tv, NULL);
    snprintf(app_id, sizeof(app_id), APP_NAME "_%lx", tv.tv_sec << 32 | tv.tv_usec);

    // setup window position via Sway IPC
    const int ipc = sway_connect();
    if (ipc != -1) {
        bool sway_fullscreen = false;
        if (!viewer.wnd.width) {
            // get currently focused window state
            sway_current(ipc, &viewer.wnd, &sway_fullscreen);
        }
        viewer.fullscreen |= sway_fullscreen;
        if (!viewer.fullscreen && viewer.wnd.width) {
            sway_add_rules(ipc, app_id, viewer.wnd.x, viewer.wnd.y);
        }
        sway_disconnect(ipc);
    }

    // create and show GUI window
    if (!create_window(&handlers, viewer.wnd.width, viewer.wnd.height, app_id)) {
        goto done;
    }
    if (!load_next_file(true, false)) {
        goto done;
    }
    if (viewer.fullscreen) {
        enable_fullscreen(true);
    }
    show_window();
    rc = true;

done:
    // clean
    destroy_window();
    free_image(ctx.image);

    return rc;
}
