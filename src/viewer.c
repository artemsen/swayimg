// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "viewer.h"

#include "buildcfg.h"
#include "canvas.h"
#include "config.h"
#include "image.h"
#include "sway.h"
#include "text.h"
#include "window.h"

#include <stdlib.h>
#include <string.h>

/** Viewer context. */
typedef struct {
    config_t* config;    ///< Configuration
    file_list_t* files;  ///< List of files to view
    image_t* image;      ///< Currently displayed image
    text_render_t* text; ///< Text renderer
    canvas_t canvas;     ///< Canvas context
} viewer_t;

static viewer_t viewer;

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
    apply_scale(&viewer.canvas, viewer.image->surface, viewer.config->scale);

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
    if (viewer.image) {
        // not an initial call, move to the next file
        bool moved = forward ? file_list_next(viewer.files)
                             : file_list_prev(viewer.files);
        if (!moved) {
            return false;
        }
    }

    while (!load_image(file_list_current(viewer.files, NULL, NULL))) {
        if (!file_list_skip(viewer.files)) {
            fprintf(stderr, "No more image files to view\n");
            return false;
        }
    }

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
        if (viewer.config->background == BACKGROUND_GRID) {
            draw_grid(&viewer.canvas, viewer.image->surface, cairo);
        } else {
            cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
            cairo_set_source_rgb(cairo, RGB_RED(viewer.config->background),
                                 RGB_GREEN(viewer.config->background),
                                 RGB_BLUE(viewer.config->background));
            cairo_paint(cairo);
        }
    }
    draw_image(&viewer.canvas, viewer.image->surface, cairo);

    // image meta information: file name, format, exif, etc
    if (viewer.config->show_info && viewer.text) {
        char text[32];
        // print meta info
        print_text(viewer.text, cairo, text_top_left, viewer.image->info);
        // print current scale
        snprintf(text, sizeof(text), "%i%%",
                 (int)(viewer.canvas.scale * 100.0));
        print_text(viewer.text, cairo, text_bottom_left, text);
        // print file number in list
        if (viewer.files) {
            size_t index, total;
            file_list_current(viewer.files, &index, &total);
            if (total > 1) {
                snprintf(text, sizeof(text), "%lu of %lu", index, total);
                print_text(viewer.text, cairo, text_top_right, text);
            }
        }
    }

    cairo_destroy(cairo);
}

/** Window resize handler, see handlers::on_resize */
static void on_resize(void)
{
    viewer.canvas.scale = 0.0; // to force recalculate considering window size
    apply_scale(&viewer.canvas, viewer.image->surface, viewer.config->scale);
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
                               viewer.config->scale);
        case XKB_KEY_i:
            viewer.config->show_info = !viewer.config->show_info;
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
            viewer.config->fullscreen = !viewer.config->fullscreen;
            enable_fullscreen(viewer.config->fullscreen);
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

bool run_viewer(config_t* cfg, file_list_t* files)
{
    static const struct handlers handlers = { .on_redraw = on_redraw,
                                              .on_resize = on_resize,
                                              .on_keyboard = on_keyboard };
    bool rc = false;

    // initialize viewer context
    viewer.config = cfg;
    viewer.files = files;
    viewer.text = init_text(cfg);

    if (cfg->sway_wm) {
        // setup window position via Sway IPC
        bool sway_fullscreen = false;
        const int ipc = sway_connect();
        if (ipc != -1) {
            if (!cfg->window.width) {
                // get currently focused window state
                sway_current(ipc, &cfg->window, &sway_fullscreen);
            }
            cfg->fullscreen |= sway_fullscreen;
            if (!cfg->fullscreen && cfg->window.width) {
                sway_add_rules(ipc, cfg->app_id, cfg->window.x, cfg->window.y);
            }
            sway_disconnect(ipc);
        }
    }

    // GUI prepare
    if (!create_window(&handlers, cfg->window.width, cfg->window.height,
                       cfg->app_id)) {
        goto done;
    }

    // load first file
    if (!files) {
        // stdin mode
        if (!load_image(NULL)) {
            goto done;
        }
    } else if (!next_file(true)) {
        goto done;
    }

    if (cfg->fullscreen) {
        enable_fullscreen(true);
    }

    // GUI loop
    show_window();

    rc = true;

done:
    destroy_window();
    image_free(viewer.image);
    free_text(viewer.text);

    return rc;
}
