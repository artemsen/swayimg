// SPDX-License-Identifier: MIT
// Business logic of application.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "viewer.h"

#include "buildcfg.h"
#include "canvas.h"
#include "config.h"
#include "image.h"
#include "window.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KIBIBYTE 1024
#define MEBIBYTE (KIBIBYTE * 1024)

#define MAX_DESC_LINES 16 // Max number of lines in description table
#define MAX_DESC_LEN   16 // Max lenght of description values

/** Image description. */
struct image_desc {
    char frame_size[MAX_DESC_LEN]; ///< Buffer for frame size info text
    char frame_index[64];          ///< Buffer for frame index info text
    char file_size[MAX_DESC_LEN];  ///< Buffer for file size info text
    struct info_table table[MAX_DESC_LINES]; ///< Info table
    size_t size; ///< Total number of lines in the table
};

/** Viewer context. */
struct viewer {
    struct config* config;   ///< Configuration
    struct image_list* list; ///< List of images to view
    struct canvas* canvas;   ///< Canvas context
    size_t frame;            ///< Index of current frame
    bool animation;          ///< Animation is in progress
    struct image_desc desc;  ///< Text image description
};

/**
 * Reset image view state, recalculate position and scale.
 * @param ctx viewer context
 */
static void reset_viewport(struct viewer* ctx)
{
    const struct image_entry entry = image_list_current(ctx->list);
    const struct image_frame* frame = &entry.image->frames[ctx->frame];
    enum canvas_scale scale;

    switch (ctx->config->scale) {
        case cfgsc_fit:
            scale = cs_fit_window;
            break;
        case cfgsc_real:
            scale = cs_real_size;
            break;
        default:
            scale = cs_fit_or100;
    }
    canvas_reset_image(ctx->canvas, frame->width, frame->height, scale);
}

/**
 * Handle frame switch.
 * @param ctx viewer context
 */
static void on_frame_switched(struct viewer* ctx)
{
    const struct image_entry entry = image_list_current(ctx->list);
    const struct image_frame* frame = &entry.image->frames[ctx->frame];

    snprintf(ctx->desc.frame_size, sizeof(ctx->desc.frame_size), "%lux%lu",
             frame->width, frame->height);

    if (entry.image->num_frames > 1) {
        snprintf(ctx->desc.frame_index, sizeof(ctx->desc.frame_index),
                 "%lu of %lu", ctx->frame + 1, entry.image->num_frames);
    }
}

/**
 * Switch to the next or previous frame.
 * @param ctx viewer context
 * @param forward switch direction
 * @return false if there is only one frame in the image
 */
static bool switch_frame(struct viewer* ctx, bool forward)
{
    size_t index = ctx->frame;
    const struct image_entry entry = image_list_current(ctx->list);

    if (forward) {
        if (++index >= entry.image->num_frames) {
            index = 0;
        }
    } else {
        if (index-- == 0) {
            index = entry.image->num_frames - 1;
        }
    }

    if (index == ctx->frame) {
        return false;
    }

    ctx->frame = index;
    on_frame_switched(ctx);

    return true;
}

/**
 * Start or stop animation.
 * @param ctx viewer context
 * @param start state to set
 */
static void animation_control(struct viewer* ctx, bool start)
{
    if (!start) {
        ctx->animation = false;
    } else {
        const struct image_entry entry = image_list_current(ctx->list);
        const size_t duration = entry.image->frames[ctx->frame].duration;
        if (entry.image->num_frames > 1 && duration) {
            ctx->animation = true;
            add_callback(duration);
        }
    }
}

/**
 * File load handler.
 * @param ctx viewer context
 */
static void on_file_loaded(struct viewer* ctx)
{
    struct info_table* table = ctx->desc.table;
    const struct image_entry entry = image_list_current(ctx->list);

    ctx->animation = false;
    ctx->frame = 0;
    ctx->desc.size = 0;

    // fill image description
    table[ctx->desc.size].key = "File";
    table[ctx->desc.size++].value = entry.image->file_name;

    table[ctx->desc.size].key = "File size";
    table[ctx->desc.size++].value = ctx->desc.file_size;
    if (entry.image->file_size >= MEBIBYTE) {
        snprintf(ctx->desc.file_size, sizeof(ctx->desc.file_size), "%.02f MiB",
                 (float)entry.image->file_size / MEBIBYTE);
    } else {
        snprintf(ctx->desc.file_size, sizeof(ctx->desc.file_size), "%.02f KiB",
                 (float)entry.image->file_size / KIBIBYTE);
    }

    table[ctx->desc.size].key = "Format";
    table[ctx->desc.size++].value = entry.image->format;

    // EXIF
    for (size_t i = 0; i < entry.image->num_info; ++i) {
        if (ctx->desc.size >= MAX_DESC_LINES) {
            break;
        }
        table[ctx->desc.size].key = entry.image->info[i].key;
        table[ctx->desc.size++].value = entry.image->info[i].value;
    }

    if (ctx->desc.size < MAX_DESC_LINES) {
        table[ctx->desc.size].key = "Image size";
        table[ctx->desc.size++].value = ctx->desc.frame_size;
    }

    if (ctx->desc.size < MAX_DESC_LINES && entry.image->num_frames > 1) {
        table[ctx->desc.size].key = "Frame";
        table[ctx->desc.size++].value = ctx->desc.frame_index;
    }

    on_frame_switched(ctx);
    reset_viewport(ctx);
    set_window_title(entry.image->file_name);
    animation_control(ctx, true);
}

/**
 * Load image file.
 * @param ctx viewer context
 * @param jump position to set
 * @return false if file was not loaded
 */
static bool load_file(struct viewer* ctx, enum list_jump jump)
{
    if (!image_list_jump(ctx->list, jump)) {
        return false;
    }
    on_file_loaded(ctx);
    return true;
}

/** Draw handler, see wnd_handlers::on_redraw */
static void on_redraw(void* data, argb_t* window)
{
    struct viewer* ctx = data;
    const struct image_entry entry = image_list_current(ctx->list);

    canvas_clear(ctx->canvas, window);
    canvas_draw_image(ctx->canvas, entry.image->alpha,
                      entry.image->frames[ctx->frame].data, window);

    // image meta information: file name, format, exif, etc
    if (ctx->config->show_info) {
        char text[32];
        const int scale = canvas_get_scale(ctx->canvas) * 100;

        // print meta info
        canvas_print_info(ctx->canvas, window, ctx->desc.size, ctx->desc.table);

        // print current scale
        snprintf(text, sizeof(text), "%d%%", scale);
        canvas_print_line(ctx->canvas, window, cc_bottom_left, text);
        // print file number in list
        if (image_list_size(ctx->list) > 1) {
            snprintf(text, sizeof(text), "%lu of %lu", entry.index + 1,
                     image_list_size(ctx->list));
            canvas_print_line(ctx->canvas, window, cc_top_right, text);
        }
    }

    if (ctx->config->mark_mode) {
        if (entry.marked) {
            canvas_print_line(ctx->canvas, window, cc_bottom_right, "MARKED");
        }
    }
}

/** Window resize handler, see wnd_handlers::on_resize */
static void on_resize(void* data, size_t width, size_t height)
{
    struct viewer* ctx = data;
    canvas_resize_window(ctx->canvas, width, height);
    reset_viewport(ctx);
}

/** Keyboard handler, see wnd_handlers::on_keyboard. */
static bool on_keyboard(void* data, xkb_keysym_t key)
{
    struct viewer* ctx = data;

    switch (key) {
        case XKB_KEY_SunPageUp:
        case XKB_KEY_p:
            return load_file(ctx, jump_prev_file);
        case XKB_KEY_SunPageDown:
        case XKB_KEY_n:
        case XKB_KEY_space:
            return load_file(ctx, jump_next_file);
        case XKB_KEY_P:
            return load_file(ctx, jump_prev_dir);
        case XKB_KEY_N:
            return load_file(ctx, jump_next_dir);
        case XKB_KEY_Home:
        case XKB_KEY_g:
            return load_file(ctx, jump_first_file);
        case XKB_KEY_End:
        case XKB_KEY_G:
            return load_file(ctx, jump_last_file);
        case XKB_KEY_O:
        case XKB_KEY_F2:
            ctx->animation = false;
            return switch_frame(ctx, false);
        case XKB_KEY_o:
        case XKB_KEY_F3:
            ctx->animation = false;
            return switch_frame(ctx, true);
        case XKB_KEY_s:
        case XKB_KEY_F4:
            animation_control(ctx, !ctx->animation);
            return true;
        case XKB_KEY_Left:
        case XKB_KEY_h:
            return canvas_move(ctx->canvas, cm_step_left);
        case XKB_KEY_Right:
        case XKB_KEY_l:
            return canvas_move(ctx->canvas, cm_step_right);
        case XKB_KEY_Up:
        case XKB_KEY_k:
            return canvas_move(ctx->canvas, cm_step_up);
        case XKB_KEY_Down:
        case XKB_KEY_j:
            return canvas_move(ctx->canvas, cm_step_down);
        case XKB_KEY_equal:
        case XKB_KEY_plus:
            canvas_set_scale(ctx->canvas, cs_zoom_in);
            return true;
        case XKB_KEY_minus:
            canvas_set_scale(ctx->canvas, cs_zoom_out);
            return true;
        case XKB_KEY_0:
            canvas_set_scale(ctx->canvas, cs_real_size);
            return true;
        case XKB_KEY_BackSpace:
            reset_viewport(ctx);
            return true;
        case XKB_KEY_i:
            ctx->config->show_info = !ctx->config->show_info;
            return true;
        case XKB_KEY_Insert:
        case XKB_KEY_m:
            image_list_mark_invcur(ctx->list);
            return true;
        case XKB_KEY_asterisk:
        case XKB_KEY_M:
            image_list_mark_invall(ctx->list);
            return true;
        case XKB_KEY_a:
            image_list_mark_setall(ctx->list, true);
            return true;
        case XKB_KEY_A:
            image_list_mark_setall(ctx->list, false);
            return true;
        case XKB_KEY_F5:
        case XKB_KEY_bracketleft:
            image_rotate(image_list_current(ctx->list).image, 270);
            canvas_swap_image_size(ctx->canvas);
            return true;
        case XKB_KEY_F6:
        case XKB_KEY_bracketright:
            image_rotate(image_list_current(ctx->list).image, 90);
            canvas_swap_image_size(ctx->canvas);
            return true;
        case XKB_KEY_F7:
            image_flip_vertical(image_list_current(ctx->list).image);
            return true;
        case XKB_KEY_F8:
            image_flip_horizontal(image_list_current(ctx->list).image);
            return true;
        case XKB_KEY_F11:
        case XKB_KEY_f:
            ctx->config->fullscreen = !ctx->config->fullscreen;
            enable_fullscreen(ctx->config->fullscreen);
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

/** Keyboard handler, see wnd_handlers::on_timer. */
static void on_timer(void* data)
{
    struct viewer* ctx = data;
    if (ctx->animation) {
        const struct image_entry entry = image_list_current(ctx->list);
        const struct image_frame* frame = &entry.image->frames[ctx->frame];
        add_callback(frame->duration);
        switch_frame(ctx, true);
    }
}

bool run_viewer(struct config* cfg, struct image_list* list)
{
    bool rc = false;
    struct viewer ctx = {
        .config = cfg,
        .list = list,
        .canvas = canvas_init(cfg),
    };
    struct wnd_handlers handlers = {
        .on_redraw = on_redraw,
        .on_resize = on_resize,
        .on_keyboard = on_keyboard,
        .on_timer = on_timer,
        .data = &ctx,
    };

    if (!ctx.canvas) {
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
    on_file_loaded(&ctx);
    show_window();
    destroy_window();

    rc = true;

done:
    canvas_free(ctx.canvas);
    return rc;
}
