// SPDX-License-Identifier: MIT
// Business logic of application.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "viewer.h"

#include "buildcfg.h"
#include "canvas.h"
#include "config.h"
#include "image.h"
#include "window.h"

#include <stdlib.h>
#include <string.h>

/** Viewer context. */
struct viewer {
    struct config* config;   ///< Configuration
    struct file_list* files; ///< List of files to view
    struct image* image;     ///< Currently displayed image
    struct canvas* canvas;   ///< Canvas context
};
static struct viewer viewer;

/** Reset image view state, recalculate position and scale. */
static void reset_viewport(void)
{
    enum canvas_scale scale;

    switch (viewer.config->scale) {
        case cfgsc_fit:
            scale = cs_fit_window;
            break;
        case cfgsc_real:
            scale = cs_real_size;
            break;
        default:
            scale = cs_fit_or100;
    }
    canvas_reset_image(viewer.canvas, viewer.image->width, viewer.image->height,
                       scale);

    set_window_title(viewer.image->path);
}

/**
 * Load next image file.
 * @param file true for next file, false for next directory
 * @param forward true to move forward, false to backward
 * @return false if file was not loaded
 */
static bool load_next(bool file, bool forward)
{
    struct image* image = NULL;

    if (!viewer.files) {
        return false;
    }

    if (viewer.image) { // don't move on first call
        bool moved = file ? flist_next_file(viewer.files, forward)
                          : flist_next_directory(viewer.files, forward);
        if (!moved) {
            return false;
        }
    }

    while (!image) {
        image = image_from_file(flist_current(viewer.files, NULL, NULL));
        if (!image && !flist_exclude(viewer.files, forward)) {
            fprintf(stderr, "No more image files to view\n");
            return false;
        }
    }

    image_free(viewer.image);
    viewer.image = image;

    reset_viewport();

    return true;
}

/** Draw handler, see wnd_handlers::on_redraw */
static void on_redraw(argb_t* wnd)
{
    canvas_clear(viewer.canvas, wnd);
    canvas_draw_image(viewer.canvas, viewer.image->alpha, viewer.image->data,
                      wnd);

    // image meta information: file name, format, exif, etc
    if (viewer.config->show_info) {
        char text[32];
        const int scale = canvas_get_scale(viewer.canvas) * 100;
        // print meta info
        canvas_print_meta(viewer.canvas, wnd, viewer.image->info);
        // print current scale
        snprintf(text, sizeof(text), "%d%%", scale);
        canvas_print_line(viewer.canvas, wnd, cc_bottom_left, text);
        // print file number in list
        if (viewer.files) {
            size_t index, total;
            flist_current(viewer.files, &index, &total);
            if (total > 1) {
                snprintf(text, sizeof(text), "%lu of %lu", index, total);
                canvas_print_line(viewer.canvas, wnd, cc_top_right, text);
            }
        }
    }
}

/** Window resize handler, see wnd_handlers::on_resize */
static void on_resize(size_t width, size_t height)
{
    if (canvas_resize_window(viewer.canvas, width, height)) {
        reset_viewport();
    }
}

/** Keyboard handler, see wnd_handlers::on_keyboard. */
static bool on_keyboard(xkb_keysym_t key)
{
    switch (key) {
        case XKB_KEY_SunPageUp:
        case XKB_KEY_p:
            return load_next(true, false);
        case XKB_KEY_SunPageDown:
        case XKB_KEY_n:
        case XKB_KEY_space:
            return load_next(true, true);
        case XKB_KEY_P:
            return load_next(false, false);
        case XKB_KEY_N:
            return load_next(false, true);
        case XKB_KEY_Left:
        case XKB_KEY_h:
            return canvas_move(viewer.canvas, cm_step_left);
        case XKB_KEY_Right:
        case XKB_KEY_l:
            return canvas_move(viewer.canvas, cm_step_right);
        case XKB_KEY_Up:
        case XKB_KEY_k:
            return canvas_move(viewer.canvas, cm_step_up);
        case XKB_KEY_Down:
        case XKB_KEY_j:
            return canvas_move(viewer.canvas, cm_step_down);
        case XKB_KEY_equal:
        case XKB_KEY_plus:
            canvas_set_scale(viewer.canvas, cs_zoom_in);
            return true;
        case XKB_KEY_minus:
            canvas_set_scale(viewer.canvas, cs_zoom_out);
            return true;
        case XKB_KEY_0:
            canvas_set_scale(viewer.canvas, cs_real_size);
            return true;
        case XKB_KEY_BackSpace:
            canvas_set_scale(viewer.canvas, cs_fit_or100);
            return true;
        case XKB_KEY_i:
            viewer.config->show_info = !viewer.config->show_info;
            return true;
        case XKB_KEY_F5:
        case XKB_KEY_bracketleft:
            image_rotate(viewer.image, 270);
            canvas_swap_image_size(viewer.canvas);
            return true;
        case XKB_KEY_F6:
        case XKB_KEY_bracketright:
            image_rotate(viewer.image, 90);
            canvas_swap_image_size(viewer.canvas);
            return true;
        case XKB_KEY_F7:
            image_flip_vertical(viewer.image);
            return true;
        case XKB_KEY_F8:
            image_flip_horizontal(viewer.image);
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

bool run_viewer(struct config* cfg, struct file_list* files)
{
    bool rc = false;
    static const struct wnd_handlers handlers = { .on_redraw = on_redraw,
                                                  .on_resize = on_resize,
                                                  .on_keyboard = on_keyboard };
    // initialize viewer context
    viewer.config = cfg;
    viewer.files = files;
    viewer.canvas = canvas_init(cfg);
    if (!viewer.canvas) {
        goto done;
    }

    // load first file
    if (!files) {
        viewer.image = image_from_stdin();
        if (!viewer.image) {
            goto done;
        }
    } else if (!load_next(true, true)) {
        goto done;
    }

    // Start GUI
    if (!create_window(&handlers, cfg->window.width, cfg->window.height,
                       cfg->app_id)) {
        goto done;
    }
    if (cfg->fullscreen) {
        enable_fullscreen(true);
    }
    show_window();
    destroy_window();

    rc = true;

done:
    image_free(viewer.image);
    canvas_free(viewer.canvas);

    return rc;
}
