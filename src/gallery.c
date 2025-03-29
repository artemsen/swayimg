// SPDX-License-Identifier: MIT
// Gallery mode.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "gallery.h"

#include "application.h"
#include "imagelist.h"
#include "info.h"
#include "ui.h"

#include <stdlib.h>

// Scale for selected thumbnail
#define THUMB_SELECTED_SCALE 1.15f

/** Gallery context. */
struct gallery {
    size_t thumb_size;     ///< Size of thumbnail
    size_t thumb_cache;    ///< Max number of thumbnails in cache
    enum aa_mode thumb_aa; ///< Anti-aliasing mode
    bool thumb_fill;       ///< Scale mode (fill/fit)
    bool thumb_pstore;     ///< Use persistent storage for thumbnails

    argb_t clr_window;     ///< Window background
    argb_t clr_background; ///< Tile background
    argb_t clr_select;     ///< Selected tile background
    argb_t clr_border;     ///< Selected tile border
    argb_t clr_shadow;     ///< Selected tile shadow

    struct image* selected; ///< Currently selected image
    struct image* top;      ///< Top left (first displayed) image

    //////////////////////////////////////////////////////////////////////////
    size_t top_OLD;      ///< Index of the first displayed image
    size_t selected_OLD; ///< Index of the selected image
};

/** Global gallery context. */
static struct gallery ctx;

/**
 * Get thumbnail layout.
 * @param cols,rows,gap layout description
 */
static void get_layout(size_t* cols, size_t* rows, size_t* gap)
{
    const size_t width = ui_get_width();
    const size_t height = ui_get_height();
    const size_t cnum = width / ctx.thumb_size;
    const size_t gap_px = (width - (cnum * ctx.thumb_size)) / (cnum + 1);
    const size_t rnum = (height - gap_px) / (ctx.thumb_size + gap_px);

    if (cols) {
        *cols = cnum;
    }
    if (rows) {
        *rows = rnum;
    }
    if (gap) {
        *gap = gap_px;
    }
}

/** Reset loader queue. */
// static void reset_loader(void)
// {
//     // get number of thumbnails on the screen
//     // size_t cols, rows;
//     // get_layout(&cols, &rows, NULL);
//     // ++rows;
//     // const size_t total = cols * rows;

//     // // search for nearest to selected
//     // const size_t last = image_list_jump(ctx.top_OLD, total - 1, true);
//     // const size_t max_f = image_list_distance(ctx.selected_OLD, last);
//     // const size_t max_b = image_list_distance(ctx.top_OLD,
//     ctx.selected_OLD);

//     // size_t next_f = ctx.selected_OLD;
//     // size_t next_b = ctx.selected_OLD;

//     // loader_queue_reset();
//     // if (!thumbnail_get(ctx.selected_OLD)) {
//     //     loader_queue_append(ctx.selected_OLD);
//     // }

//     // for (size_t i = 0; i < max(max_f, max_b); ++i) {
//     //     if (i < max_f) {
//     //         next_f = image_list_nearest(next_f, true, false);
//     //         if (!thumbnail_get(next_f)) {
//     //             loader_queue_append(next_f);
//     //         }
//     //     }
//     //     if (i < max_b) {
//     //         next_b = image_list_nearest(next_b, false, false);
//     //         if (!thumbnail_get(next_b)) {
//     //             loader_queue_append(next_b);
//     //         }
//     //     }
//     // }

//     // // remove the furthest thumbnails from the cache
//     // if (ctx.thumb_cache != 0 && total < ctx.thumb_cache) {
//     //     const size_t half = (ctx.thumb_cache - total) / 2;
//     //     const size_t min_id = image_list_jump(ctx.top_OLD, half, false);
//     //     const size_t max_id = image_list_jump(last, half, true);
//     //     thumbnail_clear(min_id, max_id);
//     // }
// }

/** Update thumbnail layout. */
static void update_layout(void)
{
    // size_t cols, rows;
    // size_t distance;

    // get_layout(&cols, &rows, NULL);

    // // if selection is not visible, put it on the center
    // distance = image_list_distance(ctx.top_OLD, ctx.selected_OLD);
    // if (distance > cols * rows) {
    //     const size_t center_x = cols / 2;
    //     const size_t center_y = rows / 2;
    //     ctx.top_OLD = image_list_jump(ctx.selected_OLD,
    //                                   center_y * cols + center_x, false);
    // }

    // // remove gap at the bottom of the screen
    // distance = image_list_distance(ctx.top_OLD, image_list_last());
    // if (distance < cols * (rows - 1)) {
    //     ctx.top_OLD =
    //         image_list_jump(image_list_last(), cols * rows - 1, false);
    // }

    // reset_loader();
}

/**
 * Update info text container for currently selected image.
 */
// static void update_info(void)
// {
//     // const struct thumbnail* th = thumbnail_get(ctx.selected_OLD);

//     // if (th) {
//     //     info_reset(th->image);
//     //     info_update(info_image_size, "%zux%zu", th->width, th->height);
//     //     info_update(info_index, "%zu of %zu", th->image->index + 1,
//     //                 image_list_size());
//     // }

//     // app_redraw();
// }

/**
 * Set current selection.
 * @param index image index to set as selected one
 */
// static void select_thumbnail(size_t index)
// {
//     ctx.selected_OLD = index;
//     update_info();
//     update_layout();
//     app_redraw();
// }

/**
 * Skip specified image.
 * @param index image position in the image list
 * @return true if next image was loaded
 */
static bool skip_thumbnail(size_t index)
{
    (void)index;
    // const size_t next = image_list_skip(index);

    // if (next == IMGLIST_INVALID) {
    //     printf("No more images, exit\n");
    //     app_exit(0);
    //     return false;
    // }

    // thumbnail_remove(index);

    // if (index == ctx.top_OLD || ctx.top_OLD > next) {
    //     ctx.top_OLD = next;
    // }
    // if (index == ctx.selected_OLD) {
    //     select_thumbnail(next);
    // } else {
    //     update_layout();
    // }

    return true;
}

/**
 * Select closest item.
 * @param direction next image position in list
 */
static void select_next(enum action_type direction)
{
    (void)direction;
    // size_t cols, rows;
    // size_t index;

    // get_layout(&cols, &rows, NULL);

    // index = ctx.selected_OLD;

    // switch (direction) {
    //     case action_first_file:
    //         index = image_list_first();
    //         ctx.top_OLD = index;
    //         break;
    //     case action_last_file:
    //         index = image_list_last();
    //         ctx.top_OLD = image_list_jump(index, cols * rows - 1, false);
    //         break;
    //     case action_prev_file:
    //     case action_step_left:
    //         if (index != image_list_first()) {
    //             index = image_list_nearest(index, false, false);
    //         }
    //         break;
    //     case action_next_file:
    //     case action_step_right:
    //         if (index != image_list_last()) {
    //             index = image_list_nearest(index, true, false);
    //         }
    //         break;
    //     case action_step_up:
    //         index = image_list_jump(index, cols, false);
    //         break;
    //     case action_step_down:
    //         index = image_list_jump(index, cols, true);
    //         break;
    //     default:
    //         break;
    // }

    // if (index != IMGLIST_INVALID && index != ctx.selected_OLD) {
    //     // fix up top by one line
    //     if (ctx.top_OLD > index) {
    //         ctx.top_OLD = image_list_jump(ctx.top_OLD, cols, false);
    //     } else {
    //         const size_t distance = image_list_distance(ctx.top_OLD, index);
    //         if (distance >= cols * rows) {
    //             ctx.top_OLD = image_list_jump(ctx.top_OLD, cols, true);
    //         }
    //     }
    //     select_thumbnail(index);
    // }
}

/**
 * Scroll one page.
 * @param forward scroll direction
 */
static void scroll_page(bool forward)
{
    (void)forward;
    // size_t cols, rows;
    // size_t distance;
    // size_t selected;

    // get_layout(&cols, &rows, NULL);
    // distance = cols * rows - 1;
    // selected = image_list_jump(ctx.selected_OLD, distance, forward);

    // if (selected != IMGLIST_INVALID && selected != ctx.selected_OLD) {
    //     const size_t top = image_list_jump(ctx.top_OLD, distance, forward);
    //     if (top != image_list_last()) {
    //         ctx.top_OLD = top;
    //     }
    //     select_thumbnail(selected);
    // }
}

/**
 * Draw thumbnail.
 * @param window destination window
 * @param x,y top left coordinate
 * @param img thumbnail image
 */
static void draw_thumbnail(struct pixmap* window, size_t x, size_t y,
                           const struct image* img)
{
    const struct pixmap* thumb = img ? &img->thumbnail : NULL;

    if (img != ctx.selected) {
        pixmap_fill(window, x, y, ctx.thumb_size, ctx.thumb_size,
                    ctx.clr_background);
        if (thumb) {
            x += ctx.thumb_size / 2 - thumb->width / 2;
            y += ctx.thumb_size / 2 - thumb->height / 2;
            pixmap_copy(thumb, window, x, y, img->alpha);
        }
    } else {
        // currently selected item
        const size_t thumb_size = THUMB_SELECTED_SCALE * ctx.thumb_size;
        const size_t thumb_offset = (thumb_size - ctx.thumb_size) / 2;

        x = max(0, x - (ssize_t)thumb_offset);
        y = max(0, y - (ssize_t)thumb_offset);
        if (x + thumb_size >= window->width) {
            x = window->width - thumb_size;
        }

        pixmap_fill(window, x, y, thumb_size, thumb_size, ctx.clr_select);

        if (thumb) {
            const ssize_t thumb_w = thumb->width * THUMB_SELECTED_SCALE;
            const ssize_t thumb_h = thumb->height * THUMB_SELECTED_SCALE;
            const ssize_t tx = x + thumb_size / 2 - thumb_w / 2;
            const ssize_t ty = y + thumb_size / 2 - thumb_h / 2;
            pixmap_scale(ctx.thumb_aa, thumb, window, tx, ty,
                         THUMB_SELECTED_SCALE, img->alpha);
        }

        // shadow
        if (ARGB_GET_A(ctx.clr_shadow)) {
            const argb_t base = ctx.clr_shadow & 0x00ffffff;
            const uint8_t alpha = ARGB_GET_A(ctx.clr_shadow);
            const size_t width =
                max(1, (double)thumb_size / 15.0 * ((double)alpha / 255.0));
            const size_t alpha_step = alpha / width;

            for (size_t i = 0; i < width; ++i) {
                const ssize_t lx = i + x + thumb_size;
                const ssize_t ly = y + width;
                const ssize_t lh = thumb_size - (width - i);
                const argb_t color = base | ARGB_SET_A(alpha - i * alpha_step);
                pixmap_vline(window, lx, ly, lh, color);
            }
            for (size_t i = 0; i < width; ++i) {
                const ssize_t lx = x + width;
                const ssize_t ly = y + thumb_size + i;
                const ssize_t lw = thumb_size - (width - i) + 1;
                const argb_t color = base | ARGB_SET_A(alpha - i * alpha_step);
                pixmap_hline(window, lx, ly, lw, color);
            }
        }

        // border
        if (ARGB_GET_A(ctx.clr_border)) {
            pixmap_rect(window, x, y, thumb_size, thumb_size, ctx.clr_border);
        }
    }
}

/**
 * Draw thumbnails.
 * @param window destination window
 */
static void draw_thumbnails(struct pixmap* window)
{
    size_t cols, rows, gap;
    size_t selected_x = 0, selected_y = 0;
    struct image* img;

    // thumbnail layout
    get_layout(&cols, &rows, &gap);
    ++rows; // show images that do not fit completely in the window

    if (!ctx.top) {
        // calculate the first image taking into account the placement
        // of the selected image in the middle of the window
        ctx.top = imglist_back(ctx.selected, cols * rows / 2);
    }

    // draw
    img = ctx.top;
    for (size_t row = 0; img && row < rows; ++row) {
        const size_t y = row * ctx.thumb_size + gap * (row + 1);
        for (size_t col = 0; img && col < cols; ++col) {
            const size_t x = col * ctx.thumb_size + gap * (col + 1);
            struct image* next;

            if (img != ctx.selected) {
                draw_thumbnail(window, x, y, img);
            } else {
                // currently selected image is drawn last
                selected_x = x;
                selected_y = y;
            }

            next = imglist_next(img);
            image_free(img);
            img = next;
        }
    }

    if (img) {
        image_free(img);
    }

    draw_thumbnail(window, selected_x, selected_y, ctx.selected);
}

/** Mode handler: window redraw. */
static void on_redraw(struct pixmap* window)
{
    pixmap_fill(window, 0, 0, window->width, window->height, ctx.clr_window);
    draw_thumbnails(window);
    // info_print(window);
}

/** Mode handler: apply action. */
static void on_action(const struct action* action)
{
    switch (action->type) {
        case action_antialiasing:
            ctx.thumb_aa = aa_switch(ctx.thumb_aa, action->params);
            info_update(info_status, "Anti-aliasing: %s",
                        aa_name(ctx.thumb_aa));
            // thumbnail_clear(IMGLIST_INVALID, IMGLIST_INVALID);
            // reset_loader();
            app_redraw();
            break;
        case action_first_file:
        case action_last_file:
        case action_prev_file:
        case action_next_file:
        case action_step_left:
        case action_step_right:
        case action_step_up:
        case action_step_down:
            select_next(action->type);
            break;
        case action_page_up:
        case action_page_down:
            ////////////////////////////////////////////////////////////////
            puts("NOT IMPL");
            abort();
            scroll_page(action->type == action_page_down);
            break;
        case action_skip_file:
            ////////////////////////////////////////////////////////////////
            puts("NOT IMPL");
            abort();
            skip_thumbnail(ctx.selected_OLD);
            break;
        case action_reload:
            ////////////////////////////////////////////////////////////////
            puts("NOT IMPL");
            abort();
            // thumbnail_clear(IMGLIST_INVALID, IMGLIST_INVALID);
            // reset_loader();
            // app_redraw();
            break;
        default:
            break;
    }
}

/** Mode handler: get currently viewed image. */
static struct image* on_current(void)
{
    return ctx.selected;
}

/** Mode handler: activate viewer. */
static void on_activate(struct image* image)
{
    ctx.selected = image;
    ctx.top = NULL;
}

/** Mode handler: deactivate viewer. */
static struct image* on_deactivate(void)
{
    return ctx.selected;
}

void gallery_init(const struct config* cfg, struct mode_handlers* handlers)
{
    ctx.thumb_size = config_get_num(cfg, CFG_GALLERY, CFG_GLRY_SIZE, 1, 1024);
    ctx.thumb_cache = config_get_num(cfg, CFG_GALLERY, CFG_GLRY_CACHE, 0, 1024);
    ctx.thumb_aa = aa_init(cfg, CFG_GALLERY, CFG_GLRY_AA);
    ctx.thumb_fill = config_get_bool(cfg, CFG_GALLERY, CFG_GLRY_FILL);
    ctx.thumb_pstore = config_get_bool(cfg, CFG_GALLERY, CFG_GLRY_PSTORE);

    ctx.clr_window = config_get_color(cfg, CFG_GALLERY, CFG_GLRY_WINDOW);
    ctx.clr_background = config_get_color(cfg, CFG_GALLERY, CFG_GLRY_BKG);
    ctx.clr_select = config_get_color(cfg, CFG_GALLERY, CFG_GLRY_SELECT);
    ctx.clr_border = config_get_color(cfg, CFG_GALLERY, CFG_GLRY_BORDER);
    ctx.clr_shadow = config_get_color(cfg, CFG_GALLERY, CFG_GLRY_SHADOW);

    handlers->action = on_action;
    handlers->redraw = on_redraw;
    handlers->resize = update_layout;
    handlers->current = on_current;
    handlers->activate = on_activate;
    handlers->deactivate = on_deactivate;
}

void gallery_destroy(void)
{
    // thumbnail_free();
}
