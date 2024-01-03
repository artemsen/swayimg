// SPDX-License-Identifier: MIT
// Business logic of application and UI event handlers.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "viewer.h"

#include "buildcfg.h"
#include "canvas.h"
#include "config.h"
#include "imagelist.h"
#include "info.h"
#include "keybind.h"
#include "str.h"
#include "ui.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timerfd.h>
#include <sys/wait.h>
#include <unistd.h>

/** Viewer context. */
struct viewer {
    size_t frame; ///< Index of the current frame

    bool show_help; ///< Show/hide help message

    bool animation_enable; ///< Animation enable/disable
    int animation_fd;      ///< Animation timer

    bool slideshow_enable; ///< Slideshow enable/disable
    int slideshow_fd;      ///< Slideshow timer
    size_t slideshow_time; ///< Slideshow image display time (seconds)
};

static struct viewer ctx = { .animation_enable = true,
                             .animation_fd = -1,
                             .slideshow_enable = false,
                             .slideshow_fd = -1,
                             .slideshow_time = 3 };

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

    ctx.frame = index;
    return true;
}

/**
 * Start animation if image supports it.
 * @param enable state to set
 */
static void animation_ctl(bool enable)
{
    struct itimerspec ts = { 0 };

    if (enable) {
        const struct image_entry entry = image_list_current();
        const size_t duration = entry.image->frames[ctx.frame].duration;
        enable = (entry.image->num_frames > 1 && duration);
        if (enable) {
            ts.it_value.tv_sec = duration / 1000;
            ts.it_value.tv_nsec = (duration % 1000) * 1000000;
        }
    }

    ctx.animation_enable = enable;
    timerfd_settime(ctx.animation_fd, 0, &ts, NULL);
}

/**
 * Start slide show.
 * @param enable state to set
 */
static void slideshow_ctl(bool enable)
{
    struct itimerspec ts = { 0 };

    ctx.slideshow_enable = enable;
    if (enable) {
        ts.it_value.tv_sec = ctx.slideshow_time;
    }

    timerfd_settime(ctx.slideshow_fd, 0, &ts, NULL);
}

/**
 * Reset state after loading new file.
 */
static void reset_state(void)
{
    const struct image_entry entry = image_list_current();
    const struct image_frame* frame = &entry.image->frames[0];

    ctx.frame = 0;
    canvas_reset_image(frame->width, frame->height);
    ui_set_title(entry.image->file_name);
    animation_ctl(true);
    slideshow_ctl(ctx.slideshow_enable);
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
    reset_state();
    return true;
}

/**
 * Execute system command for the current image.
 * @param expr command expression
 */
static void execute_command(const char* expr)
{
    const char* path = image_list_current().image->file_path;
    char* cmd = NULL;
    int rc = EINVAL;

    // construct command from template
    while (expr && *expr) {
        if (*expr == '%') {
            ++expr;
            if (*expr != '%') {
                str_append(path, 0, &cmd); // replace % with path
                continue;
            }
        }
        str_append(expr, 1, &cmd);
        ++expr;
    }

    if (cmd) {
        rc = system(cmd); // execute
        if (rc != -1) {
            rc = WEXITSTATUS(rc);
        } else {
            rc = errno ? errno : EINVAL;
        }
    }

    // show execution status
    if (!cmd) {
        info_set_status("Error: no command to execute");
    } else {
        if (strlen(cmd) > 30) { // trim long command
            strcpy(&cmd[27], "...");
        }
        if (rc) {
            info_set_status("Error %d: %s", rc, cmd);
        } else {
            info_set_status("OK: %s", cmd);
        }
    }

    free(cmd);

    if (!image_list_reset()) {
        printf("No more images, exit\n");
        ui_stop();
        return;
    }
    reset_state();
}

/**
 * Move viewport.
 * @param horizontal axis along which to move (false for vertical)
 * @param positive direction (increase/decrease)
 * @param params optional move step in percents
 */
static bool move_viewport(bool horizontal, bool positive, const char* params)
{
    ssize_t percent = 10;

    if (params) {
        ssize_t val;
        if (str_to_num(params, 0, &val, 0) && val > 0 && val <= 1000) {
            percent = val;
        } else {
            fprintf(stderr, "Invalid move step: \"%s\"\n", params);
        }
    }

    if (!positive) {
        percent = -percent;
    }

    return canvas_move(horizontal, percent);
}

/**
 * Animation timer event handler.
 */
static void on_animation_timer(void)
{
    next_frame(true);
    animation_ctl(true);
    ui_redraw();
}

/**
 * Slideshow timer event handler.
 */
static void on_slideshow_timer(void)
{
    slideshow_ctl(next_file(jump_next_file));
    ui_redraw();
}

/**
 * Custom section loader, see `config_loader` for details.
 */
static enum config_status load_config(const char* key, const char* value)
{
    enum config_status status = cfgst_invalid_key;

    if (strcmp(key, VIEWER_CFG_SLIDESHOW) == 0) {
        status = config_to_bool(value, &ctx.slideshow_enable)
            ? cfgst_ok
            : cfgst_invalid_value;
    } else if (strcmp(key, VIEWER_CFG_SLIDESHOW_TIME) == 0) {
        ssize_t num;
        if (str_to_num(value, 0, &num, 0) && num != 0 && num <= 86400) {
            ctx.slideshow_time = num;
            status = cfgst_ok;
        } else {
            status = cfgst_invalid_value;
        }
    }

    return status;
}

void viewer_init(void)
{
    // setup animation timer
    ctx.animation_fd =
        timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (ctx.animation_fd != -1) {
        ui_add_event(ctx.animation_fd, on_animation_timer);
    }

    // setup slideshow timer
    ctx.slideshow_fd =
        timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (ctx.slideshow_fd != -1) {
        ui_add_event(ctx.slideshow_fd, on_slideshow_timer);
    }

    // register configuration loader
    config_add_loader(GENERAL_CONFIG_SECTION, load_config);
}

void viewer_free(void)
{
    if (ctx.animation_fd != -1) {
        close(ctx.animation_fd);
    }
    if (ctx.slideshow_fd != -1) {
        close(ctx.slideshow_fd);
    }
}

void viewer_reset(void)
{
    if (image_list_reset()) {
        reset_state();
        info_set_status("Image reloaded");
        ui_redraw();
    } else {
        printf("No more images, exit\n");
        ui_stop();
    }
}

void viewer_on_redraw(argb_t* window)
{
    const struct image_entry entry = image_list_current();

    canvas_clear(window);
    canvas_draw_image(entry.image->alpha, entry.image->frames[ctx.frame].data,
                      window);

    info_update(ctx.frame);
    for (size_t i = 0; i < INFO_POSITION_NUM; ++i) {
        const size_t lines_num = info_height(i);
        if (lines_num) {
            const enum info_position pos = (enum info_position)i;
            const struct info_line* lines = info_lines(pos);
            canvas_print(lines, lines_num, pos, window);
        }
    }

    if (ctx.show_help) {
        const wchar_t** lines = malloc(key_bindings_size * sizeof(wchar_t*));
        if (lines) {
            size_t lines_num = 0;
            for (size_t i = 0; i < key_bindings_size; ++i) {
                if (key_bindings[i].help) {
                    lines[lines_num++] = key_bindings[i].help;
                }
            }
            canvas_print_center(lines, lines_num, window);
            free(lines);
        }
    }

    // reset one-time rendered notification message
    info_set_status(NULL);
}

void viewer_on_resize(size_t width, size_t height, size_t scale)
{
    canvas_reset_window(width, height, scale);
    reset_state();
}

void viewer_on_keyboard(xkb_keysym_t key)
{
    bool redraw = false;
    const struct key_binding* kbind = keybind_get(key);

    if (!kbind) {
        return;
    }

    // handle action
    switch (kbind->action) {
        case kb_none:
            break;
        case kb_help:
            ctx.show_help = !ctx.show_help;
            redraw = true;
            break;
        case kb_first_file:
            redraw = next_file(jump_first_file);
            break;
        case kb_last_file:
            redraw = next_file(jump_last_file);
            break;
        case kb_prev_dir:
            redraw = next_file(jump_prev_dir);
            break;
        case kb_next_dir:
            redraw = next_file(jump_next_dir);
            break;
        case kb_prev_file:
            redraw = next_file(jump_prev_file);
            break;
        case kb_next_file:
            redraw = next_file(jump_next_file);
            break;
        case kb_prev_frame:
        case kb_next_frame:
            animation_ctl(false);
            redraw = next_frame(kbind->action == kb_next_frame);
            break;
        case kb_animation:
            animation_ctl(!ctx.animation_enable);
            break;
        case kb_slideshow:
            slideshow_ctl(!ctx.slideshow_enable && next_file(jump_next_file));
            redraw = true;
            break;
        case kb_fullscreen:
            ui_toggle_fullscreen();
            break;
        case kb_step_left:
            redraw = move_viewport(true, true, kbind->params);
            break;
        case kb_step_right:
            redraw = move_viewport(true, false, kbind->params);
            break;
        case kb_step_up:
            redraw = move_viewport(false, true, kbind->params);
            break;
        case kb_step_down:
            redraw = move_viewport(false, false, kbind->params);
            break;
        case kb_zoom:
            canvas_zoom(kbind->params);
            redraw = true;
            break;
        case kb_rotate_left:
            image_rotate(image_list_current().image, 270);
            canvas_swap_image_size();
            redraw = true;
            break;
        case kb_rotate_right:
            image_rotate(image_list_current().image, 90);
            canvas_swap_image_size();
            redraw = true;
            break;
        case kb_flip_vertical:
            image_flip_vertical(image_list_current().image);
            redraw = true;
            break;
        case kb_flip_horizontal:
            image_flip_horizontal(image_list_current().image);
            redraw = true;
            break;
        case kb_antialiasing:
            info_set_status("Anti-aliasing %s",
                            canvas_switch_aa() ? "on" : "off");
            redraw = true;
            break;
        case kb_reload:
            viewer_reset();
            break;
        case kb_info:
            info_set_mode(kbind->params);
            redraw = true;
            break;
        case kb_exec:
            execute_command(kbind->params);
            redraw = true;
            break;
        case kb_exit:
            ui_stop();
            break;
    }

    if (redraw) {
        ui_redraw();
    }
}
