// SPDX-License-Identifier: MIT
// Business logic of application.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "viewer.h"

#include "buildcfg.h"
#include "canvas.h"

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
    bool slideshow;          ///< Slideshow is in progress
    struct image_desc desc;  ///< Text image description
};

/**
 * Set current frame.
 * @param ctx viewer context
 * @param frame target frame index
 */
static void set_frame(struct viewer* ctx, size_t index)
{
    const struct image_entry entry = image_list_current(ctx->list);
    const struct image_frame* frame = &entry.image->frames[index];

    ctx->frame = index;

    // update image description text
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
static bool next_frame(struct viewer* ctx, bool forward)
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

    set_frame(ctx, index);
    return true;
}

/**
 * Start slide show.
 * @param ctx viewer context
 * @param ui UI context
 * @param enable state to set
 */
static void slideshow_ctl(struct viewer* ctx, struct ui* ui, bool enable)
{
    ctx->slideshow = enable;
    if (enable) {
        ui_set_timer(ui, ui_timer_slideshow, ctx->config->slideshow_sec * 1000);
    }
}

/**
 * Start animation if image supports it.
 * @param ctx viewer context
 * @param ui UI context
 * @param enable state to set
 */
static void animation_ctl(struct viewer* ctx, struct ui* ui, bool enable)
{
    if (enable) {
        const struct image_entry entry = image_list_current(ctx->list);
        const size_t duration = entry.image->frames[ctx->frame].duration;
        ctx->animation = (entry.image->num_frames > 1 && duration);
        if (ctx->animation) {
            ui_set_timer(ui, ui_timer_animation, duration);
        }
    } else {
        ctx->animation = false;
    }
}

/**
 * Update window title.
 * @param ctx viewer context
 * @param ui UI context
 */
static void update_window_title(struct viewer* ctx, struct ui* ui)
{
    const char* prefix = APP_NAME ": ";
    const struct image_entry entry = image_list_current(ctx->list);
    const size_t len = strlen(prefix) + strlen(entry.image->file_name) + 1;
    char* title = malloc(len);

    if (title) {
        strcpy(title, prefix);
        strcat(title, entry.image->file_name);
        ui_set_title(ui, title);
        free(title);
    }
}

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
 * Reset state after loading new file.
 * @param ctx viewer context
 * @param ui UI context
 */
static void reset_state(struct viewer* ctx, struct ui* ui)
{
    const struct image_entry entry = image_list_current(ctx->list);
    const struct image* image = entry.image;
    struct image_desc* desc = &ctx->desc;
    struct info_table* table = desc->table;

    // update image description
    desc->size = 0;
    table[desc->size].key = "File";
    table[desc->size++].value = image->file_name;
    table[desc->size].key = "File size";
    table[desc->size++].value = desc->file_size;
    snprintf(desc->file_size, sizeof(desc->file_size), "%.02f %ciB",
             (float)image->file_size /
                 (image->file_size >= MEBIBYTE ? MEBIBYTE : KIBIBYTE),
             (image->file_size >= MEBIBYTE ? 'M' : 'K'));
    table[desc->size].key = "Format";
    table[desc->size++].value = image->format;
    // exif and other meat data
    for (size_t i = 0; i < image->num_info; ++i) {
        if (desc->size >= MAX_DESC_LINES) {
            break;
        }
        table[desc->size].key = image->info[i].key;
        table[desc->size++].value = image->info[i].value;
    }
    // dynamic fields
    if (desc->size < MAX_DESC_LINES) {
        table[desc->size].key = "Image size";
        table[desc->size++].value = desc->frame_size;
    }
    if (desc->size < MAX_DESC_LINES && image->num_frames > 1) {
        table[desc->size].key = "Frame";
        table[desc->size++].value = desc->frame_index;
    }

    ctx->animation = false;
    set_frame(ctx, 0);
    reset_viewport(ctx);
    update_window_title(ctx, ui);
    animation_ctl(ctx, ui, true);
}

/**
 * Load next file.
 * @param ctx viewer context
 * @param ui UI context
 * @param jump position of the next file in list
 * @return false if file was not loaded
 */
static bool next_file(struct viewer* ctx, struct ui* ui, enum list_jump jump)
{
    if (!image_list_jump(ctx->list, jump)) {
        return false;
    }
    reset_state(ctx, ui);
    return true;
}

struct viewer* viewer_create(struct config* cfg, struct image_list* list,
                             struct ui* ui)
{
    struct viewer* ctx;

    ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        fprintf(stderr, "Not enough memory\n");
        return NULL;
    }

    ctx->config = cfg;
    ctx->list = list;
    ctx->canvas = canvas_init(cfg);

    if (!ctx->canvas) {
        viewer_free(ctx);
        return NULL;
    }

    if (cfg->slideshow) {
        slideshow_ctl(ctx, ui, true); // start slide show
    }

    return ctx;
}

void viewer_free(struct viewer* ctx)
{
    if (ctx) {
        canvas_free(ctx->canvas);
        free(ctx);
    }
}

void viewer_on_redraw(void* data, argb_t* window)
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

void viewer_on_resize(void* data, struct ui* ui, size_t width, size_t height,
                      size_t scale)
{
    struct viewer* ctx = data;

    canvas_resize_window(ctx->canvas, width, height, scale);
    reset_viewport(ctx);
    reset_state(ctx, ui);
}

bool viewer_on_keyboard(void* data, struct ui* ui, xkb_keysym_t key)
{
    struct viewer* ctx = data;

    switch (key) {
        case XKB_KEY_SunPageUp:
        case XKB_KEY_p:
            return next_file(ctx, ui, jump_prev_file);
        case XKB_KEY_SunPageDown:
        case XKB_KEY_n:
        case XKB_KEY_space:
            return next_file(ctx, ui, jump_next_file);
        case XKB_KEY_P:
            return next_file(ctx, ui, jump_prev_dir);
        case XKB_KEY_N:
            return next_file(ctx, ui, jump_next_dir);
        case XKB_KEY_Home:
        case XKB_KEY_g:
            return next_file(ctx, ui, jump_first_file);
        case XKB_KEY_End:
        case XKB_KEY_G:
            return next_file(ctx, ui, jump_last_file);
        case XKB_KEY_O:
        case XKB_KEY_F2:
            slideshow_ctl(ctx, ui, false);
            animation_ctl(ctx, ui, false);
            return next_frame(ctx, false);
        case XKB_KEY_o:
        case XKB_KEY_F3:
            slideshow_ctl(ctx, ui, false);
            animation_ctl(ctx, ui, false);
            return next_frame(ctx, true);
        case XKB_KEY_s:
        case XKB_KEY_F4:
            animation_ctl(ctx, ui, !ctx->animation);
            return false;
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
        case XKB_KEY_F9:
            slideshow_ctl(
                ctx, ui, !ctx->slideshow && next_file(ctx, ui, jump_next_file));
            return true;
        case XKB_KEY_F11:
        case XKB_KEY_f:
            ctx->config->fullscreen = !ctx->config->fullscreen;
            ui_set_fullscreen(ui, ctx->config->fullscreen);
            return false;
        case XKB_KEY_Escape:
        case XKB_KEY_Return:
        case XKB_KEY_F10:
        case XKB_KEY_q:
            ui_stop(ui);
            return false;
    }
    return false;
}

void viewer_on_timer(void* data, enum ui_timer timer, struct ui* ui)
{
    struct viewer* ctx = data;

    if (timer == ui_timer_slideshow && ctx->slideshow &&
        next_file(ctx, ui, jump_next_file)) {
        slideshow_ctl(ctx, ui, true);
    }

    if (timer == ui_timer_animation && ctx->animation) {
        next_frame(ctx, true);
        animation_ctl(ctx, ui, true);
    }
}
