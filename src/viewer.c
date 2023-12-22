// SPDX-License-Identifier: MIT
// Business logic of application and UI event handlers.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "viewer.h"

#include "buildcfg.h"
#include "canvas.h"
#include "config.h"
#include "imagelist.h"
#include "ui.h"

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
    size_t frame;           ///< Index of current frame
    bool animation;         ///< Animation is in progress
    bool slideshow;         ///< Slideshow is in progress
    struct image_desc desc; ///< Text image description
    char* message;          ///< One-time rendered notification message
};
static struct viewer ctx;

/**
 * Set current frame.
 * @param frame target frame index
 */
static void set_frame(size_t index)
{
    const struct image_entry entry = image_list_current();
    const struct image_frame* frame = &entry.image->frames[index];

    ctx.frame = index;

    // update image description text
    snprintf(ctx.desc.frame_size, sizeof(ctx.desc.frame_size), "%lux%lu",
             frame->width, frame->height);
    if (entry.image->num_frames > 1) {
        snprintf(ctx.desc.frame_index, sizeof(ctx.desc.frame_index),
                 "%lu of %lu", ctx.frame + 1, entry.image->num_frames);
    }
}

/**
 * Switch to the next or previous frame.
 * @param forward switch direction
 * @return false if there is only one frame in the image
 */
static bool next_frame(bool forward)
{
    size_t index = ctx.frame;
    const struct image_entry entry = image_list_current();

    if (forward) {
        if (++index >= entry.image->num_frames) {
            index = 0;
        }
    } else {
        if (index-- == 0) {
            index = entry.image->num_frames - 1;
        }
    }
    if (index == ctx.frame) {
        return false;
    }

    set_frame(index);
    return true;
}

/**
 * Start slide show.
 * @param enable state to set
 */
static void slideshow_ctl(bool enable)
{
    ctx.slideshow = enable;
    if (enable) {
        ui_set_timer(ui_timer_slideshow, config.slideshow_sec * 1000);
    }
}

/**
 * Start animation if image supports it.
 * @param enable state to set
 */
static void animation_ctl(bool enable)
{
    if (enable) {
        const struct image_entry entry = image_list_current();
        const size_t duration = entry.image->frames[ctx.frame].duration;
        ctx.animation = (entry.image->num_frames > 1 && duration);
        if (ctx.animation) {
            ui_set_timer(ui_timer_animation, duration);
        }
    } else {
        ctx.animation = false;
    }
}

/**
 * Update window title.
 */
static void update_window_title(void)
{
    const char* prefix = APP_NAME ": ";
    const struct image_entry entry = image_list_current();
    const size_t len = strlen(prefix) + strlen(entry.image->file_name) + 1;
    char* title = malloc(len);

    if (title) {
        strcpy(title, prefix);
        strcat(title, entry.image->file_name);
        ui_set_title(title);
        free(title);
    }
}

/**
 * Reset image view state, recalculate position and scale.
 */
static void reset_viewport(void)
{
    const struct image_entry entry = image_list_current();
    const struct image_frame* frame = &entry.image->frames[ctx.frame];
    enum canvas_scale scale;

    switch (config.scale) {
        case cfgsc_fit:
            scale = cs_fit_window;
            break;
        case cfgsc_fill:
            scale = cs_fill_window;
            break;
        case cfgsc_real:
            scale = cs_real_size;
            break;
        default:
            scale = cs_fit_or100;
    }
    canvas_reset_image(frame->width, frame->height, scale);
}

/**
 * Reset state after loading new file.
 */
static void reset_state(void)
{
    const struct image_entry entry = image_list_current();
    const struct image* image = entry.image;
    struct image_desc* desc = &ctx.desc;
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

    ctx.animation = false;
    set_frame(0);
    reset_viewport();
    update_window_title();
    animation_ctl(true);
}

/**
 * Load next file.
 * @param jump position of the next file in list
 * @return false if file was not loaded
 */
static bool next_file(enum list_jump jump)
{
    if (!image_list_jump(jump)) {
        return false;
    }
    slideshow_ctl(ctx.slideshow);
    reset_state();
    return true;
}

/**
 * Set one-time rendered notification message.
 * @param fmt message format description
 */
__attribute__((format(printf, 1, 2))) static void set_message(const char* fmt,
                                                              ...)
{
    va_list args;
    int len;

    if (ctx.message) {
        free(ctx.message);
        ctx.message = NULL;
    }

    va_start(args, fmt);
    len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (len <= 0) {
        return;
    }
    ++len; // last null
    ctx.message = malloc(len);
    if (ctx.message) {
        va_start(args, fmt);
        vsprintf(ctx.message, fmt, args);
        va_end(args);
    }
}

void viewer_init(void)
{
    if (config.slideshow) {
        slideshow_ctl(true); // start slide show
    }
}

void viewer_free(void)
{
    free(ctx.message);
}

void viewer_on_redraw(argb_t* window)
{
    const struct image_entry entry = image_list_current();

    canvas_clear(window);
    canvas_draw_image(entry.image->alpha, entry.image->frames[ctx.frame].data,
                      window);

    // image meta information: file name, format, exif, etc
    if (config.show_info) {
        char text[32];
        const int scale = canvas_get_scale() * 100;

        // print meta info
        canvas_print_info(window, ctx.desc.size, ctx.desc.table);

        // print current scale
        snprintf(text, sizeof(text), "%d%%", scale);
        canvas_print_line(window, cc_bottom_left, text);
        // print file number in list
        if (image_list_size() > 1) {
            snprintf(text, sizeof(text), "%lu of %lu", entry.index + 1,
                     image_list_size());
            canvas_print_line(window, cc_top_right, text);
        }
    }

    // one-time rendered notification message
    if (ctx.message) {
        canvas_print_line(window, cc_bottom_right, ctx.message);
        free(ctx.message);
        ctx.message = NULL;
    }
}

void viewer_on_resize(size_t width, size_t height, size_t scale)
{
    canvas_reset_window(width, height, scale);
    reset_viewport();
    reset_state();
}

bool viewer_on_keyboard(xkb_keysym_t key)
{
    enum config_action action = cfgact_none;

    // get action binded to the key
    for (size_t i = 0; i < MAX_KEYBINDINGS; ++i) {
        struct config_keybind* bind = &config.keybind[i];
        if (bind->key == key) {
            action = bind->action;
            break;
        }
        if (bind->key == XKB_KEY_NoSymbol) {
            return false; // end of binding table
        }
    }

    // handle action
    switch (action) {
        case cfgact_none:
            return false;
        case cfgact_first_file:
            return next_file(jump_first_file);
        case cfgact_last_file:
            return next_file(jump_last_file);
        case cfgact_prev_dir:
            return next_file(jump_prev_dir);
        case cfgact_next_dir:
            return next_file(jump_next_dir);
        case cfgact_prev_file:
            return next_file(jump_prev_file);
        case cfgact_next_file:
            return next_file(jump_next_file);
        case cfgact_prev_frame:
        case cfgact_next_frame:
            slideshow_ctl(false);
            animation_ctl(false);
            return next_frame(action == cfgact_next_frame);
        case cfgact_animation:
            animation_ctl(!ctx.animation);
            return false;
        case cfgact_slideshow:
            slideshow_ctl(!ctx.slideshow && next_file(jump_next_file));
            return true;
        case cfgact_fullscreen:
            config.fullscreen = !config.fullscreen;
            ui_set_fullscreen(config.fullscreen);
            return false;
        case cfgact_step_left:
            return canvas_move(cm_step_left);
        case cfgact_step_right:
            return canvas_move(cm_step_right);
        case cfgact_step_up:
            return canvas_move(cm_step_up);
        case cfgact_step_down:
            return canvas_move(cm_step_down);
        case cfgact_zoom_in:
            canvas_set_scale(cs_zoom_in);
            return true;
        case cfgact_zoom_out:
            canvas_set_scale(cs_zoom_out);
            return true;
        case cfgact_zoom_optimal:
            canvas_set_scale(cs_fit_or100);
            return true;
        case cfgact_zoom_fit:
            canvas_set_scale(cs_fit_window);
            return true;
        case cfgact_zoom_fill:
            canvas_set_scale(cs_fill_window);
            return true;
        case cfgact_zoom_real:
            canvas_set_scale(cs_real_size);
            return true;
        case cfgact_zoom_reset:
            reset_viewport();
            return true;
        case cfgact_rotate_left:
            image_rotate(image_list_current().image, 270);
            canvas_swap_image_size();
            return true;
        case cfgact_rotate_right:
            image_rotate(image_list_current().image, 90);
            canvas_swap_image_size();
            return true;
        case cfgact_flip_vertical:
            image_flip_vertical(image_list_current().image);
            return true;
        case cfgact_flip_horizontal:
            image_flip_horizontal(image_list_current().image);
            return true;
        case cfgact_antialiasing:
            config.antialiasing = !config.antialiasing;
            set_message("Anti-aliasing %s", config.antialiasing ? "on" : "off");
            return true;
        case cfgact_reload:
            if (image_list_reset()) {
                reset_state();
                set_message("Image reloaded");
                return true;
            } else {
                printf("No more images, exit\n");
                ui_stop();
                return false;
            }
        case cfgact_info:
            config.show_info = !config.show_info;
            return true;
        case cfgact_exec: {
            const int rc = image_list_exec();
            if (rc) {
                set_message("Execute failed: code %d", rc);
            } else {
                set_message("Execute success");
            }
            if (!image_list_reset()) {
                printf("No more images, exit\n");
                ui_stop();
                return false;
            }
            reset_state();
            return true;
        }
        case cfgact_quit:
            ui_stop();
            return false;
    }
    return false;
}

void viewer_on_anim_timer(void)
{
    if (ctx.animation) {
        next_frame(true);
        animation_ctl(true);
    }
}

void viewer_on_ss_timer(void)
{
    if (ctx.slideshow && next_file(jump_next_file)) {
        slideshow_ctl(true);
    }
}
