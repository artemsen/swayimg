// SPDX-License-Identifier: MIT
// Gallery mode.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "gallery.h"

#include "application.h"
#include "imagelist.h"
#include "info.h"
#include "loader.h"
#include "str.h"
#include "text.h"
#include "ui.h"

#include <stdlib.h>
#include <string.h>

// Scale for selected thumbnail
#define THUMB_SELECTED_SCALE 1.15f

/** List of thumbnails. */
struct thumbnail {
    struct image* image;    ///< Preview image
    size_t width, height;   ///< Real image size
    struct thumbnail* next; ///< Next entry
};

/** Gallery context. */
struct gallery {
    size_t thumb_size;        ///< Max size of thumbnail
    struct thumbnail* thumbs; ///< List of preview images
    bool thumb_aa;            ///< Use anti-aliasing for thumbnail

    argb_t clr_window;     ///< Window background
    argb_t clr_background; ///< Tile background
    argb_t clr_select;     ///< Seleted tile background

    int load_complete; ///< Thumbnail load notification
    pthread_t loader;  ///< Thumbnail loader thread

    size_t top;      ///< Index of the first displayed image
    size_t selected; ///< Index of the selected image

    struct text_surface path; ///< File path text surface
};

/** Global gallery context. */
static struct gallery ctx;

/**
 * Reset thumbnail list and free its resources.
 */
static void reset_thumbnails(void)
{
    while (ctx.thumbs) {
        struct thumbnail* entry = ctx.thumbs;
        ctx.thumbs = ctx.thumbs->next;
        image_free(entry->image);
        free(entry);
    }
    app_redraw();
}

/**
 * Add new thumbnail from existing image.
 * @param image original image
 */
static void add_thumbnail(struct image* image)
{
    struct thumbnail* entry = malloc(sizeof(*entry));
    if (entry) {
        entry->width = image->frames[0].pm.width;
        entry->height = image->frames[0].pm.height;
        entry->image = image;

        // create thumbnail from image
        image_thumbnail(image, ctx.thumb_size, ctx.thumb_aa);

        // add to the list
        entry->next = ctx.thumbs;
        ctx.thumbs = entry;
    }
}

/** Background loader thread callback. */
static size_t on_image_loaded(struct image* image, size_t index)
{
    if (image) {
        add_thumbnail(image);
        notification_raise(ctx.load_complete);
    } else {
        const size_t next = image_list_skip(index);
        if (next > index) {
            return next;
        }
    }
    return IMGLIST_INVALID;
}

/**
 * Get thumbnail.
 * @param index preferable position in the image list
 * @return thumbnail instance or NULL if no more files in the list
 */
static struct thumbnail* get_thumbnail(size_t index)
{
    struct thumbnail* it;

    if (index == IMGLIST_INVALID) {
        index = image_list_first();
    }

    // check if next file entry available
    if (!image_list_get(index)) {
        const size_t next = image_list_next_file(index);
        if (next == IMGLIST_INVALID || next < index) {
            return NULL; // no more files
        }
        index = next;
    }

    // search for already loaded thumbnail
    it = ctx.thumbs;
    while (it) {
        if (it->image->index == index) {
            return it;
        }
        it = it->next;
    }

    load_image_start(index, NULL, on_image_loaded);

    return NULL;
}

/**
 * Get thumbnail layout.
 * @param cols,rows,margin layout description
 */
static void get_layout(size_t* cols, size_t* rows, size_t* margin)
{
    const size_t wnd_width = ui_get_width();
    const size_t wnd_height = ui_get_height();

    *cols = wnd_width / ctx.thumb_size;
    *rows = wnd_height / ctx.thumb_size;
    *margin = (wnd_width - (*cols * ctx.thumb_size)) / (*cols + 1);

    if (*rows * ctx.thumb_size + *margin * (*rows + 1) < wnd_height) {
        ++(*rows);
    }
}

/**
 * Draw frame with shadow for selected thumbnail.
 * @param window destination window
 * @param x,y top left coordinate
 * @param size size of the frame
 */
static void draw_frame(struct pixmap* window, size_t x, size_t y, size_t size)
{
    const size_t shadow_width = max(1, ctx.thumb_size / 15);
    const size_t alpha_step = 0xff / shadow_width;

    // shadow
    for (size_t i = 0; i < shadow_width; ++i) {
        const ssize_t lx = i + x + size;
        const ssize_t ly = y + shadow_width;
        const ssize_t lh = size - (shadow_width - i);
        const argb_t color = ARGB_SET_A(0xff - i * alpha_step);
        pixmap_vline(window, lx, ly, lh, color);
    }
    for (size_t i = 0; i < shadow_width; ++i) {
        const ssize_t lx = x + shadow_width;
        const ssize_t ly = y + size + i;
        const ssize_t lw = size - (shadow_width - i) + 1;
        const argb_t color = ARGB_SET_A(0xff - i * alpha_step);
        pixmap_hline(window, lx, ly, lw, color);
    }

    // border
    pixmap_rect(window, x, y, size, size, ARGB_SET_A(0xff));
}

/**
 * Draw thumbnails.
 * @param window destination window
 */
static void draw_thumbnails(struct pixmap* window)
{
    size_t index = ctx.top;
    size_t cols, rows, margin;
    ssize_t sel_x, sel_y;
    const struct image* sel_th = NULL;

    get_layout(&cols, &rows, &margin);

    for (size_t row = 0; row < rows; ++row) {
        const ssize_t y = row * ctx.thumb_size + margin * (row + 1);
        for (size_t col = 0; col < cols; ++col) {
            const ssize_t x = col * ctx.thumb_size + margin * (col + 1);
            const struct thumbnail* th = get_thumbnail(index);
            if (!th) {
                row = rows; // break parent loop
                break;
            }

            // draw preview, but postpone the selected item
            if (th->image->index == ctx.selected) {
                sel_x = x;
                sel_y = y;
                sel_th = th->image;
            } else {
                const struct pixmap* img = &th->image->frames[0].pm;
                const ssize_t tx = x + ctx.thumb_size / 2 - img->width / 2;
                const ssize_t ty = y + ctx.thumb_size / 2 - img->height / 2;
                pixmap_fill(window, x, y, ctx.thumb_size, ctx.thumb_size,
                            ctx.clr_background);
                pixmap_copy(img, window, tx, ty, th->image->alpha);
            }

            index = image_list_next_file(th->image->index);
            if (index <= ctx.top) {
                row = rows; // break parent loop
                break;
            }
        }
    }

    // draw selected thumbnail
    if (sel_th) {
        const struct pixmap* img = &sel_th->frames[0].pm;
        const size_t thumb_size = THUMB_SELECTED_SCALE * ctx.thumb_size;
        const size_t offset = (thumb_size - ctx.thumb_size) / 2;
        size_t tx, ty;

        sel_x -= offset;
        sel_y -= offset;

        if (sel_y < 0) {
            sel_y = 0;
        }
        if (sel_x < 0) {
            sel_x = 0;
        }
        if (sel_x + thumb_size >= window->width) {
            sel_x = window->width - thumb_size;
        }

        tx = sel_x + ctx.thumb_size / 2 - img->width / 2;
        ty = sel_y + ctx.thumb_size / 2 - img->height / 2;

        pixmap_fill(window, sel_x, sel_y, thumb_size, thumb_size,
                    ctx.clr_select);

        if (ctx.thumb_aa) {
            pixmap_scale_bicubic(img, window, tx, ty, THUMB_SELECTED_SCALE,
                                 sel_th->alpha);
        } else {
            pixmap_scale_nearest(img, window, tx, ty, THUMB_SELECTED_SCALE,
                                 sel_th->alpha);
        }

        draw_frame(window, sel_x, sel_y, thumb_size);
    }
}

/**
 * Draw gallery.
 */
static void redraw(void)
{
    struct pixmap* wnd = ui_draw_begin();
    if (!wnd) {
        return;
    }

    pixmap_fill(wnd, 0, 0, wnd->width, wnd->height, ctx.clr_window);
    draw_thumbnails(wnd);
    info_print(wnd);

    ui_draw_commit();
}

/**
 * Set current selection.
 * @param index image index to set as selected one
 */
static void set_selection(size_t index)
{
    size_t cols, rows, margin;
    struct thumbnail* th = get_thumbnail(index);

    ctx.selected = index;

    if (th) {
        info_reset(th->image);
        info_update(info_image_size, "%zux%zu", th->width, th->height);
        info_update(info_index, "%zu of %zu", th->image->index + 1,
                    image_list_size());
    }

    get_layout(&cols, &rows, &margin);

    // rewind thumbnail list down
    if (ctx.selected > ctx.top + cols) {
        size_t pos = 0;
        size_t distance = 0;
        size_t second_line = 0;
        size_t index = ctx.top;
        for (size_t row = 0; row < rows; ++row) {
            for (size_t col = 0; col < cols; ++col) {
                if (index == ctx.selected) {
                    distance = pos;
                    break;
                }
                if (row == 1 && col == 0) {
                    second_line = index;
                }
                index = image_list_next_file(index);
                ++pos;
            }
        }
        if (distance >= cols * (rows - 1)) {
            ctx.top = second_line;
        }
    }

    // rewind thumbnail list up
    if (ctx.selected < ctx.top) {
        size_t index = ctx.top;
        size_t i = cols;
        while (i-- && index != IMGLIST_INVALID) {
            index = image_list_prev_file(index);
        }
        ctx.top = index;
    }
}

/**
 * Select next item.
 * @param direction next image position in list
 */
static void move_selection(enum action_type direction)
{
    size_t cols, rows, margin;
    size_t index = ctx.selected;

    get_layout(&cols, &rows, &margin);

    switch (direction) {
        case action_first_file:
            index = image_list_first();
            ctx.top = index;
            break;
        // case action_last_file:
        //     index = image_list_last();
        //     ctx.top = index; // todo
        //     break;
        case action_prev_file:
        case action_step_left:
            if (index != image_list_first()) {
                index = image_list_prev_file(index);
            }
            break;
        case action_next_file:
        case action_step_right:
            if (index != image_list_last()) {
                index = image_list_next_file(index);
            }
            break;
        case action_step_up:
            if (index >= cols) {
                size_t i = cols;
                while (i-- && index != IMGLIST_INVALID) {
                    const size_t next = image_list_prev_file(index);
                    if (next > index) {
                        index = IMGLIST_INVALID;
                    } else {
                        index = next;
                    }
                }
            }
            break;
        case action_step_down:
            if (index + cols <= image_list_last()) {
                size_t i = cols;
                while (i-- && index != IMGLIST_INVALID) {
                    index = image_list_next_file(index);
                }
            }
            break;
        default:
            break;
    }

    if (index != IMGLIST_INVALID && index != ctx.selected) {
        set_selection(index);
        app_redraw();
    }
}

/**
 * Key press handler.
 * @param key code of key pressed
 * @param mods key modifiers (ctrl/alt/shift)
 */
static void on_keyboard(xkb_keysym_t key, uint8_t mods)
{
    const struct keybind* kb = keybind_get(key, mods);
    if (!kb) {
        return;
    }

    for (size_t i = 0; i < kb->num_actions; ++i) {
        const struct action* action = &kb->actions[i];
        switch (action->type) {
            case action_fullscreen:
                ui_toggle_fullscreen();
                break;
            case action_antialiasing:
                ctx.thumb_aa = !ctx.thumb_aa;
                reset_thumbnails();
                break;
            case action_first_file:
            case action_last_file:
            case action_prev_file:
            case action_next_file:
            case action_step_left:
            case action_step_right:
            case action_step_up:
            case action_step_down:
                move_selection(action->type);
                break;
            case action_reload:
                reset_thumbnails();
                break;
            case action_exec:
                app_execute(action->params, image_list_get(ctx.selected));
                break;
            case action_status:
                info_update(info_status, "%s", action->params);
                app_redraw();
                break;
            case action_mode:
                app_switch_mode(ctx.selected);
                break;
            case action_info:
                info_switch(action->params);
                app_redraw();
                break;
            case action_exit:
                app_exit(0);
                break;
            default:
                break;
        }
        ++action;
    }
}

/** Notification callback: next thumbnail is loaded. */
static void on_load_complete(__attribute__((unused)) void* data)
{
    notification_reset(ctx.load_complete);
    app_redraw();
}

/**
 * Custom section loader, see `config_loader` for details.
 */
static enum config_status load_config(const char* key, const char* value)
{
    enum config_status status = cfgst_invalid_value;

    if (strcmp(key, "size") == 0) {
        ssize_t num;
        if (str_to_num(value, 0, &num, 0) && num >= 10 && num <= 1024) {
            ctx.thumb_size = num;
            status = cfgst_ok;
        }
    } else if (strcmp(key, "window") == 0) {
        if (config_to_color(value, &ctx.clr_window)) {
            status = cfgst_ok;
        }
    } else if (strcmp(key, "background") == 0) {
        if (config_to_color(value, &ctx.clr_background)) {
            status = cfgst_ok;
        }
    } else if (strcmp(key, "select") == 0) {
        if (config_to_color(value, &ctx.clr_select)) {
            status = cfgst_ok;
        }
    } else if (strcmp(key, "antialiasing") == 0) {
        if (config_to_bool(value, &ctx.thumb_aa)) {
            status = cfgst_ok;
        }
    } else {
        status = cfgst_invalid_key;
    }

    return status;
}

void gallery_create(void)
{
    ctx.thumb_size = 200;
    ctx.top = IMGLIST_INVALID;
    ctx.selected = IMGLIST_INVALID;
    ctx.clr_background = 0xff202020;
    ctx.clr_select = 0xff404040;

    ctx.load_complete = notification_create();
    if (ctx.load_complete != -1) {
        app_watch(ctx.load_complete, on_load_complete, NULL);
    }

    // register configuration loader
    config_add_loader("gallery", load_config);
}

void gallery_init(struct image* image)
{
    if (image) {
        add_thumbnail(image);
        set_selection(image->index);
        ctx.top = image->index;
    } else {
        set_selection(image_list_first());
        ctx.top = ctx.selected;
    }
}

void gallery_destroy(void)
{
    load_image_stop();
    reset_thumbnails();
    if (ctx.load_complete != -1) {
        notification_free(ctx.load_complete);
    }
    free(ctx.path.data);
}

void gallery_handle(const struct event* event)
{
    switch (event->type) {
        case event_reload:
            reset_thumbnails();
            break;
        case event_redraw:
            redraw();
            break;
        case event_keypress:
            on_keyboard(event->param.keypress.key, event->param.keypress.mods);
            break;
        case event_activate:
            ctx.selected = event->param.activate.index;
            break;
        case event_drag:
        case event_resize:
            break; // unused in gallery mode
    }
}
