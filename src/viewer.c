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
    struct image_list* list; ///< List of images to view
    struct canvas* canvas;   ///< Canvas context
};
static struct viewer viewer;

/** Reset image view state, recalculate position and scale. */
static void reset_viewport(void)
{
    const struct image_entry entry = image_list_current(viewer.list);
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
    canvas_reset_image(viewer.canvas, entry.image->width, entry.image->height,
                       scale);

    set_window_title(entry.image->path);
}

/**
 * Load image file.
 * @param jump position to set
 * @return false if file was not loaded
 */
static bool load_file(enum list_jump jump)
{
    const bool rc = image_list_jump(viewer.list, jump);
    if (rc) {
        reset_viewport();
    }
    return rc;
}

/** Draw handler, see wnd_handlers::on_redraw */
static void on_redraw(argb_t* wnd)
{
    const struct image_entry entry = image_list_current(viewer.list);

    canvas_clear(viewer.canvas, wnd);
    canvas_draw_image(viewer.canvas, entry.image->alpha, entry.image->data,
                      wnd);

    // image meta information: file name, format, exif, etc
    if (viewer.config->show_info) {
        char text[32];
        const int scale = canvas_get_scale(viewer.canvas) * 100;
        // print meta info
        canvas_print_meta(viewer.canvas, wnd, entry.image->info);
        // print current scale
        snprintf(text, sizeof(text), "%d%%", scale);
        canvas_print_line(viewer.canvas, wnd, cc_bottom_left, text);
        // print file number in list
        if (image_list_size(viewer.list) > 1) {
            snprintf(text, sizeof(text), "%lu of %lu", entry.index + 1,
                     image_list_size(viewer.list));
            canvas_print_line(viewer.canvas, wnd, cc_top_right, text);
        }
    }

    if (viewer.config->mark_mode) {
        if (entry.marked) {
            canvas_print_line(viewer.canvas, wnd, cc_bottom_right, "MARKED");
        }
    }
}

/** Window resize handler, see wnd_handlers::on_resize */
static void on_resize(size_t width, size_t height)
{
    canvas_resize_window(viewer.canvas, width, height);
    reset_viewport();
}

/** Keyboard handler, see wnd_handlers::on_keyboard. */
static bool on_keyboard(xkb_keysym_t key)
{
    switch (key) {
        case XKB_KEY_SunPageUp:
        case XKB_KEY_p:
            return load_file(jump_prev_file);
        case XKB_KEY_SunPageDown:
        case XKB_KEY_n:
        case XKB_KEY_space:
            return load_file(jump_next_file);
        case XKB_KEY_P:
            return load_file(jump_prev_dir);
        case XKB_KEY_N:
            return load_file(jump_next_dir);
        case XKB_KEY_Home:
        case XKB_KEY_g:
            return load_file(jump_first_file);
        case XKB_KEY_End:
        case XKB_KEY_G:
            return load_file(jump_last_file);
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
            reset_viewport();
            return true;
        case XKB_KEY_i:
            viewer.config->show_info = !viewer.config->show_info;
            return true;
        case XKB_KEY_Insert:
        case XKB_KEY_m:
            image_list_mark_invcur(viewer.list);
            return true;
        case XKB_KEY_asterisk:
        case XKB_KEY_M:
            image_list_mark_invall(viewer.list);
            return true;
        case XKB_KEY_a:
            image_list_mark_setall(viewer.list, true);
            return true;
        case XKB_KEY_A:
            image_list_mark_setall(viewer.list, false);
            return true;
        case XKB_KEY_F5:
        case XKB_KEY_bracketleft:
            image_rotate(image_list_current(viewer.list).image, 270);
            canvas_swap_image_size(viewer.canvas);
            return true;
        case XKB_KEY_F6:
        case XKB_KEY_bracketright:
            image_rotate(image_list_current(viewer.list).image, 90);
            canvas_swap_image_size(viewer.canvas);
            return true;
        case XKB_KEY_F7:
            image_flip_vertical(image_list_current(viewer.list).image);
            return true;
        case XKB_KEY_F8:
            image_flip_horizontal(image_list_current(viewer.list).image);
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

bool run_viewer(struct config* cfg, struct image_list* list)
{
    bool rc = false;
    static const struct wnd_handlers handlers = { .on_redraw = on_redraw,
                                                  .on_resize = on_resize,
                                                  .on_keyboard = on_keyboard };
    // initialize viewer context
    viewer.config = cfg;
    viewer.list = list;
    viewer.canvas = canvas_init(cfg);
    if (!viewer.canvas) {
        goto done;
    }

    // Start GUI
    if (!create_window(&handlers, cfg->geometry.width, cfg->geometry.height,
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
    canvas_free(viewer.canvas);

    return rc;
}
