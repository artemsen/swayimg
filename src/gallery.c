// SPDX-License-Identifier: MIT
// Gallery mode.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "gallery.h"

#include "application.h"
#include "imagelist.h"
#include "loader.h"
#include "text.h"
#include "ui.h"

#include <pthread.h>
#include <stdlib.h>

/** List of thumbnails. */
struct thumbnail {
    struct pixmap preview;  ///< Preview image
    size_t index;           ///< Index of the image in the image list
    struct thumbnail* next; ///< Next entry
};

/** Gallery context. */
struct gallery {
    size_t thumb_size;        ///< Max size of thumbnail
    struct thumbnail* thumbs; ///< List of preview images
    size_t top;               ///< Index of the first displayed image
    size_t selected;          ///< Index of the selected displayed image
    struct info_line name;    ///< File name layer
    int load_complete;        ///< Thumbnail load notification
    pthread_t loader;         ///< Thumbnail loader thread
};

/** Global gallery context. */
static struct gallery ctx;

/**
 * Create preview from full sized image.
 * @param full image origin
 * @param preview destination preview pixmap
 */
static void create_preview(const struct pixmap* full, struct pixmap* preview)
{
    const float scale_width = 1.0 / ((float)full->width / preview->width);
    const float scale_height = 1.0 / ((float)full->height / preview->height);
    const float scale = min(scale_width, scale_height);
    const ssize_t thumb_width = scale * full->width;
    const ssize_t thumb_height = scale * full->height;
    const ssize_t thumb_x = preview->width / 2 - thumb_width / 2;
    const ssize_t thumb_y = preview->height / 2 - thumb_height / 2;

    pixmap_fill(preview, 0, 0, preview->width, preview->height, 0xff303030);
    pixmap_scale_nearest(full, preview, thumb_x, thumb_y, scale, true);
}

/**
 * Create thumbnail entry.
 * @param index image list index of the loaded image
 * @return new thumbnail instance or NULL if loading error
 */
static struct thumbnail* create_thumbnail(size_t index)
{
    struct thumbnail* entry;
    const char* source;
    struct image* image;

    // load full image
    source = image_list_get(index);
    if (!source) {
        return NULL;
    }
    image = loader_load_image(source, NULL);
    if (!image) {
        return NULL;
    }

    // create new entry
    entry = malloc(sizeof(*entry));
    if (!entry) {
        image_free(image);
        return NULL;
    }
    entry->index = index;
    entry->next = NULL;

    // create thumbnail from image
    pixmap_create(&entry->preview, ctx.thumb_size, ctx.thumb_size);
    create_preview(&image->frames[0].pm, &entry->preview);

    image_free(image);

    return entry;
}

/** Load thumbnail in background thread. */
static void* thumbnail_loader(void* data)
{
    const size_t start = (size_t)data;
    struct thumbnail* entry = NULL;
    size_t index = start;

    do {
        entry = create_thumbnail(index);
        if (!entry) {
            index = image_list_skip(index);
        }
    } while (!entry && index != IMGLIST_INVALID && index > start);

    if (entry) {
        // add to list and notify
        entry->next = ctx.thumbs;
        ctx.thumbs = entry;
        notification_raise(ctx.load_complete);
    }

    return NULL;
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
        if (it->index == index) {
            return it;
        }
        it = it->next;
    }

    // restart loader thread
    if (ctx.loader) {
        pthread_join(ctx.loader, NULL);
        ctx.loader = 0;
    }
    pthread_create(&ctx.loader, NULL, thumbnail_loader, (void*)index);

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
 * Draw selected thumbnail.
 * @param window destination window
 * @param thumb selected item thumbnail
 * @param col,row position of the selected item
 */
static void draw_selected(struct pixmap* window, const struct thumbnail* thumb,
                          size_t col, size_t row)
{
    const size_t shadow_width = ctx.thumb_size / 15;
    const size_t alpha_step = 255 / shadow_width;

    const float thumb_scale = 1.15f;
    const size_t thumb_size = thumb_scale * ctx.thumb_size;
    const size_t offset = (thumb_size - ctx.thumb_size) / 2;

    size_t cols, rows, margin;
    ssize_t thumb_x, thumb_y;

    // convert index coordinates to pixels
    get_layout(&cols, &rows, &margin);
    thumb_x = col * ctx.thumb_size + margin * (col + 1) - offset;
    thumb_y = row * ctx.thumb_size + margin * (row + 1) - offset;
    if (thumb_y < 0) {
        thumb_y = 0;
    }
    if (thumb_x < 0) {
        thumb_x = 0;
    }
    if (thumb_x + thumb_size >= window->width) {
        thumb_x = window->width - thumb_size;
    }

    // shadow
    for (size_t i = 0; i < shadow_width; ++i) {
        const ssize_t x = i + thumb_x + thumb_size;
        const ssize_t y = thumb_y + shadow_width;
        const ssize_t h = thumb_size - (shadow_width - i);
        const argb_t color = ARGB_SET_A(0xff - i * alpha_step);
        pixmap_vline(window, x, y, h, color);
    }
    for (size_t i = 0; i < shadow_width; ++i) {
        const ssize_t x = thumb_x + shadow_width;
        const ssize_t y = thumb_y + thumb_size + i;
        const ssize_t w = thumb_size - (shadow_width - i) + 1;
        const argb_t color = ARGB_SET_A(0xff - i * alpha_step);
        pixmap_hline(window, x, y, w, color);
    }

    // slightly zoomed thumbnail
    pixmap_scale_nearest(&thumb->preview, window, thumb_x, thumb_y, thumb_scale,
                         true);

    // border
    pixmap_rect(window, thumb_x, thumb_y, thumb_size + 1, thumb_size + 1,
                ARGB_SET_A(0xff));

    // print file name
    text_print(window, info_bottom_right, &ctx.name, 1);
}

/**
 * Draw thumbnails.
 * @param window destination window
 */
static void draw_thumbnails(struct pixmap* window)
{
    size_t index = ctx.top;
    size_t cols, rows, margin;
    size_t sel_x, sel_y;
    const struct thumbnail* sel_th = NULL;

    get_layout(&cols, &rows, &margin);

    for (size_t row = 0; row < rows; ++row) {
        const ssize_t y = row * ctx.thumb_size + margin * (row + 1);
        for (size_t col = 0; col < cols; ++col) {
            const struct thumbnail* th = get_thumbnail(index);
            if (!th) {
                goto done; // no more files
            }

            // draw preview, but postpone the selected item
            if (th->index == ctx.selected) {
                sel_x = col;
                sel_y = row;
                sel_th = th;
            } else {
                const ssize_t x = col * ctx.thumb_size + margin * (col + 1);
                pixmap_copy(&th->preview, window, x, y, true);
            }

            index = th->index + 1;
        }
    }

done:
    if (sel_th) {
        draw_selected(window, sel_th, sel_x, sel_y);
    }
}

/**
 * Reset thumbnail list and free its resources.
 */
static void reset_thumbnails(void)
{
    while (ctx.thumbs) {
        struct thumbnail* entry = ctx.thumbs;
        ctx.thumbs = ctx.thumbs->next;
        pixmap_free(&entry->preview);
        free(entry);
    }
}

/**
 * Draw gallery.
 */
static void redraw(void)
{
    struct pixmap* window = ui_draw_begin();
    if (window) {
        pixmap_fill(window, 0, 0, window->width, window->height, 0xff101010);
        draw_thumbnails(window);
        ui_draw_commit();
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
                    index = image_list_prev_file(index);
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
        ctx.selected = index;

        // create text layer with file name
        free(ctx.name.value.data);
        font_render(image_list_get(ctx.selected), &ctx.name.value);

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

        app_on_redraw();
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
            case action_first_file:
            // case action_last_file:
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
                app_on_redraw();
                break;
            case action_mode:
                if (loader_reset(ctx.selected, false) == ldr_success) {
                    app_switch_mode();
                }
                break;
            case action_exit:
                app_on_exit(0);
                break;
            default:
                break;
        }
        ++action;
    }
}

/** Notification callback: next thumbnail is loaded. */
static void on_load_complete(void)
{
    notification_reset(ctx.load_complete);

    if (ctx.selected == IMGLIST_INVALID) {
        move_selection(action_first_file);
    }

    app_on_redraw();
}

void gallery_create(void)
{
    // register configuration loader
    // config_add_loader(GALLERY_CONFIG_SECTION, load_config);
}

void gallery_init(void)
{
    ctx.thumb_size = 200;
    ctx.top = IMGLIST_INVALID;
    ctx.selected = IMGLIST_INVALID;

    ctx.load_complete = notification_create();
    if (ctx.load_complete != -1) {
        app_watch(ctx.load_complete, on_load_complete);
    }
}

void gallery_destroy(void)
{
    reset_thumbnails();
    if (ctx.load_complete != -1) {
        notification_free(ctx.load_complete);
    }
    if (ctx.loader) {
        pthread_join(ctx.loader, NULL);
    }
    free(ctx.name.value.data);
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
        case event_resize:
            // not supported
            break;
        case event_keypress:
            on_keyboard(event->param.keypress.key, event->param.keypress.mods);
            break;
        case event_drag:
            // not supported
            break;
        case event_activate:
            // not supported
            break;
    }
}
