// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "viewer.h"

#include "canvas.h"
#include "config.h"
#include "window.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

/** Viewer context. */
struct viewer_t viewer = { .bkg = BACKGROUND_GRID };

/**
 * Load image from file or stdin.
 * @param[in] file path to the file, NULL for reading stdin
 * @return false if image was not loaded
 */
static bool load_image(const char* file)
{
    image_t* image = NULL;
    char* title;

    if (file) {
        image = image_from_file(file);
    } else {
        image = image_from_stdin();
    }

    if (!image) {
        return false;
    }

    image_free(viewer.image);
    viewer.image = image;

    reset_canvas(&viewer.canvas);

    // fix orientation
    switch (viewer.image->orientation) {
        case ori_top_right: // flipped back-to-front
            viewer.canvas.flip = flip_horizontal;
            break;
        case ori_bottom_right: // upside down
            viewer.canvas.rotate = rotate_180;
            break;
        case ori_bottom_left: // flipped back-to-front and upside down
            viewer.canvas.flip = flip_vertical;
            break;
        case ori_left_top: // flipped back-to-front and on its side
            viewer.canvas.flip = flip_horizontal;
            viewer.canvas.rotate = rotate_90;
            break;
        case ori_right_top: // on its side
            viewer.canvas.rotate = rotate_90;
            break;
        case ori_right_bottom: // flipped back-to-front and on its far side
            viewer.canvas.flip = flip_vertical;
            viewer.canvas.rotate = rotate_270;
            break;
        case ori_left_bottom: // on its far side
            viewer.canvas.rotate = rotate_270;
            break;
        default:
            break;
    }

    // set initial scale and position of the image
    apply_scale(&viewer.canvas, viewer.image->surface, viewer.scale);

    // change window title (includes ": " and last null = 3 bytes)
    title = malloc(strlen(APP_NAME) + strlen(viewer.image->path) + 3);
    if (title) {
        strcpy(title, APP_NAME);
        strcat(title, ": ");
        strcat(title, viewer.image->path);
        set_window_title(title);
        free(title);
    }

    return true;
}

/**
 * Load next image file.
 * @param[in] forward move direction (true=forward / false=backward).
 * @return false if file was not loaded
 */
static bool next_file(bool forward)
{
    size_t index = viewer.file_list.current;
    bool initial = (viewer.image == NULL);

    if (viewer.file_list.total == 0) {
        // stdin mode, read only once
        return viewer.image || load_image(NULL);
    }

    while (true) {
        if (initial) {
            initial = false;
        } else {
            if (forward) {
                if (index < viewer.file_list.total - 1) {
                    ++index;
                } else {
                    index = 0;
                }
            } else {
                if (index > 0) {
                    --index;
                } else {
                    index = viewer.file_list.total - 1;
                }
            }
            if (index == viewer.file_list.current) {
                return false; // all files enumerated
            }
        }
        if (load_image(viewer.file_list.files[index])) {
            break;
        }
    }

    viewer.file_list.current = index;

    return true;
}

/** Draw handler, see handlers::on_redraw */
static void on_redraw(cairo_surface_t* window)
{
    cairo_t* cairo = cairo_create(window);

    // clear canvas
    cairo_set_operator(cairo, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cairo);

    // image with background
    if (cairo_image_surface_get_format(viewer.image->surface) ==
        CAIRO_FORMAT_ARGB32) {
        if (viewer.bkg == BACKGROUND_GRID) {
            draw_background(&viewer.canvas, viewer.image->surface, cairo);
        } else {
            cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
            cairo_set_source_rgb(cairo,
                                 ((double)((viewer.bkg >> 16) & 0xff) / 255.0),
                                 ((double)((viewer.bkg >> 8) & 0xff) / 255.0),
                                 ((double)(viewer.bkg & 0xff) / 255.0));
            cairo_paint(cairo);
        }
    }
    draw_image(&viewer.canvas, viewer.image->surface, cairo);

    // image meta information: file name, format, exif, etc
    if (viewer.show_info) {
        char scale[8];
        snprintf(scale, sizeof(scale), "%i%%",
                 (int)(viewer.canvas.scale * 100.0));
        draw_text(cairo, 10, get_window_height() - 30, scale);
        draw_lines(cairo, 10, 10, (const char**)viewer.image->meta);
    }

    cairo_destroy(cairo);
}

/** Window resize handler, see handlers::on_resize */
static void on_resize(void)
{
    viewer.canvas.scale = 0.0; // to force recalculate considering window size
    apply_scale(&viewer.canvas, viewer.image->surface, viewer.scale);
}

/** Keyboard handler, see handlers::on_keyboard. */
static bool on_keyboard(xkb_keysym_t key)
{
    switch (key) {
        case XKB_KEY_SunPageUp:
        case XKB_KEY_p:
            return next_file(false);
        case XKB_KEY_SunPageDown:
        case XKB_KEY_n:
        case XKB_KEY_space:
            return next_file(true);
        case XKB_KEY_Left:
        case XKB_KEY_h:
            return move_viewpoint(&viewer.canvas, viewer.image->surface,
                                  step_left);
        case XKB_KEY_Right:
        case XKB_KEY_l:
            return move_viewpoint(&viewer.canvas, viewer.image->surface,
                                  step_right);
        case XKB_KEY_Up:
        case XKB_KEY_k:
            return move_viewpoint(&viewer.canvas, viewer.image->surface,
                                  step_up);
        case XKB_KEY_Down:
        case XKB_KEY_j:
            return move_viewpoint(&viewer.canvas, viewer.image->surface,
                                  step_down);
        case XKB_KEY_equal:
        case XKB_KEY_plus:
            return apply_scale(&viewer.canvas, viewer.image->surface, zoom_in);
        case XKB_KEY_minus:
            return apply_scale(&viewer.canvas, viewer.image->surface, zoom_out);
        case XKB_KEY_0:
            return apply_scale(&viewer.canvas, viewer.image->surface,
                               scale_100);
        case XKB_KEY_BackSpace:
            return apply_scale(&viewer.canvas, viewer.image->surface,
                               viewer.scale);
        case XKB_KEY_i:
            viewer.show_info = !viewer.show_info;
            return true;
        case XKB_KEY_F5:
        case XKB_KEY_bracketleft:
            apply_rotate(&viewer.canvas, false);
            return true;
        case XKB_KEY_F6:
        case XKB_KEY_bracketright:
            apply_rotate(&viewer.canvas, true);
            return true;
        case XKB_KEY_F7:
            apply_flip(&viewer.canvas, flip_vertical);
            return true;
        case XKB_KEY_F8:
            apply_flip(&viewer.canvas, flip_horizontal);
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

bool run_viewer(void)
{
    const struct handlers handlers = { .on_redraw = on_redraw,
                                       .on_resize = on_resize,
                                       .on_keyboard = on_keyboard };
    // create unique application id
    char app_id[64];
    struct timeval tv;
    gettimeofday(&tv, NULL);
    snprintf(app_id, sizeof(app_id), APP_NAME "_%lx",
             tv.tv_sec << 32 | tv.tv_usec);

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

    // GUI prepare
    if (!create_window(&handlers, viewer.wnd.width, viewer.wnd.height,
                       app_id)) {
        return false;
    }

    // load first file
    if (!next_file(true)) {
        destroy_window();
        return false;
    }

    if (viewer.fullscreen) {
        enable_fullscreen(true);
    }

    // GUI loop
    show_window();

    destroy_window();

    image_free(viewer.image);

    return true;
}
