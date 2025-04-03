// SPDX-License-Identifier: MIT
// Gallery mode.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "gallery.h"

#include "application.h"
#include "array.h"
#include "imglist.h"
#include "info.h"
#include "ui.h"

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

    struct image* current; ///< Currently selected image
    size_t curr_col;       ///< Selected image column
    size_t curr_row;       ///< Selected image row

    size_t layout_columns; ///< Number of thumbnail columns
    size_t layout_rows;    ///< Number of thumbnail rows
    size_t layout_padding; ///< Space between thumbnails

    pthread_t loader_tid; ///< Thumbnail loader thread id
    bool loader_active;   ///< Preload in progress flag
};

/** Global gallery context. */
static struct gallery ctx;

/**
 * Remove non-visible thumbnails to save memory.
 * @param all remove even visible thumbnails
 */
static void clear_thumbnails(bool all)
{
    struct image* img;

    imglist_lock();

    if (all) {
        img = imglist_first();
        while (img) {
            image_free(img, IMGFREE_THUMB);
            img = imglist_next(img);
        }
    } else {
        const ssize_t distance_bk =
            -(ssize_t)(ctx.curr_row * ctx.layout_columns + ctx.curr_col);
        const ssize_t distance_fw =
            ctx.layout_columns * ctx.layout_rows + distance_bk;

        // from the first visible and above
        img = imglist_jump(ctx.current, distance_bk - ctx.thumb_cache / 2);
        while (img) {
            image_free(img, IMGFREE_THUMB);
            img = imglist_prev(img);
        }

        // from the last visible and below
        img = imglist_jump(ctx.current, distance_fw + ctx.thumb_cache / 2);
        while (img) {
            image_free(img, IMGFREE_THUMB);
            img = imglist_next(img);
        }
    }

    imglist_unlock();
}

/**
 * Get path for the thumbnail on persistent storage.
 * @param source original image source
 * @return path or NULL if not applicable or in case of errors
 */
static char* pstore_path(const char* source)
{
    char* path = NULL;

    if (strcmp(source, LDRSRC_STDIN) == 0 ||
        strncmp(source, LDRSRC_EXEC, LDRSRC_EXEC_LEN) == 0) {
        return NULL;
    }

    path = config_expand_path("XDG_CACHE_HOME", "/swayimg");
    if (!path) {
        path = config_expand_path("HOME", "/.cache/swayimg");
    }
    if (path) {
        char state[16];
        snprintf(state, sizeof(state), ".%04x%d%d", (uint16_t)ctx.thumb_size,
                 ctx.thumb_fill ? 1 : 0, ctx.thumb_aa);
        str_append(source, 0, &path);
        str_append(state, 0, &path);
    }

    return path;
}

/**
 * Save thumbnail on persistent storage.
 * @param img image with thumbnail to save
 */
void pstore_save(const struct image* img)
{
    char* th_path;
    char* delim;

    if (!image_has_thumb(img)) {
        return;
    }

    th_path = pstore_path(img->source);
    if (!th_path) {
        return;
    }

    // create path
    delim = th_path;
    while (true) {
        delim = strchr(delim + 1, '/');
        if (!delim) {
            break;
        }
        *delim = '\0';
        if (mkdir(th_path, S_IRWXU | S_IRWXG) && errno != EEXIST) {
            free(th_path);
            return;
        }
        *delim = '/';
    }

    image_thumb_save(img, th_path);

    free(th_path);
}

/**
 * Load thumbnail from persistent storage.
 * @param img image for loading thumbnail
 * @return true if thumbnail loaded
 */
bool pstore_load(struct image* img)
{
    bool rc = false;
    struct stat st_image;
    struct stat st_thumb;
    char* th_path;

    th_path = pstore_path(img->source);
    if (!th_path) {
        return false;
    }

    // check modification time
    if (stat(img->source, &st_image) == -1 || stat(th_path, &st_thumb) == -1 ||
        st_image.st_mtim.tv_sec > st_thumb.st_mtim.tv_sec) {
        free(th_path);
        return false;
    }

    rc = image_thumb_load(img, th_path);

    free(th_path);

    return rc;
}

/**
 * Thumbnail loader thread.
 */
static void* loader_thread(__attribute__((unused)) void* data)
{
    // number of thumbnails before and after the selected image
    const ssize_t distance_bk =
        -(ssize_t)(ctx.curr_row * ctx.layout_columns + ctx.curr_col);
    const ssize_t distance_fw =
        ctx.layout_columns * ctx.layout_rows + distance_bk;

    ssize_t step_fw = -1; // include currently selected
    ssize_t step_bk = 0;
    bool forward = false;

    while (ctx.loader_active &&
           (step_fw < distance_fw || step_bk > distance_bk)) {
        struct image* thumb;
        struct image* origin;

        // increment distance to get next image to load
        forward = !forward;
        if (forward) {
            if (step_fw >= distance_fw) {
                continue;
            }
            ++step_fw;
        } else {
            if (step_bk <= distance_bk) {
                continue;
            }
            --step_bk;
        }

        imglist_lock();

        if (forward) {
            thumb = imglist_jump(ctx.current, step_fw);
        } else {
            thumb = imglist_jump(ctx.current, step_bk);
        }
        if (!thumb || image_has_thumb(thumb)) {
            imglist_unlock();
            continue; // end of list or already loaded
        }
        if (image_thumb_create(thumb, ctx.thumb_size, ctx.thumb_fill,
                               ctx.thumb_aa)) {
            app_redraw();
            imglist_unlock();
            continue; // loaded from exiting data
        }

        // create copy to unlock the list
        thumb = image_create(thumb->source);
        imglist_unlock();
        if (!thumb) {
            break; // not enough memory
        }

        // load thumbnail
        if (!ctx.thumb_pstore || !pstore_load(thumb)) {
            if (image_load(thumb) == imgload_success) {
                image_thumb_create(thumb, ctx.thumb_size, ctx.thumb_fill,
                                   ctx.thumb_aa);
                if (ctx.thumb_pstore) {
                    // save to thumbnail to persistent storage
                    const size_t width = thumb->frames[0].pm.width;
                    const size_t height = thumb->frames[0].pm.height;
                    if (width > ctx.thumb_size && height > ctx.thumb_size) {
                        pstore_save(thumb);
                    }
                }
                image_free(thumb, IMGFREE_FRAMES); // not needed anymore
            }
        }

        imglist_lock();

        origin = imglist_find(thumb->source);
        if (!origin) {
            image_free(thumb, IMGFREE_ALL);
            imglist_unlock();
            continue;
        }

        if (image_has_thumb(thumb)) {
            image_update(origin, thumb);
        } else {
            imglist_remove(origin);
        }

        imglist_unlock();
        image_free(thumb, IMGFREE_ALL);
        app_redraw();
    }

    clear_thumbnails(false);

    ctx.loader_active = false;
    return NULL;
}

/**
 * Start thumbnail loader thread.
 */
static void loader_start(void)
{
    if (!ctx.loader_active) {
        ctx.loader_active = true;
        pthread_create(&ctx.loader_tid, NULL, loader_thread, NULL);
    }
}

/**
 * Stop thumbnail loader thread.
 */
static void loader_stop(void)
{
    if (ctx.loader_active) {
        ctx.loader_active = false;
        pthread_join(ctx.loader_tid, NULL);
    }
}

/** Update thumbnail layout info. */
static void update_layout(void)
{
    const size_t width = ui_get_width();
    const size_t height = ui_get_height();

    ctx.layout_columns = width / ctx.thumb_size;
    if (ctx.layout_columns == 0) {
        ctx.layout_columns = 1;
    }
    ctx.layout_padding = (width - (ctx.layout_columns * ctx.thumb_size)) /
        (ctx.layout_columns + 1);
    ctx.layout_rows =
        (height - ctx.layout_padding) / (ctx.thumb_size + ctx.layout_padding);
    ++ctx.layout_rows; // partly visible row
}

/**
 * Fix up selected image position.
 */
static void fixup_position(void)
{
    struct image* first;

    if (ctx.curr_col >= ctx.layout_columns ||
        ctx.curr_row >= ctx.layout_rows - 1) {
        const size_t distance = imglist_distance(imglist_first(), ctx.current);
        ctx.curr_col = distance % ctx.layout_columns;
        ctx.curr_row = (ctx.layout_rows - 1) / 2;
    }

    // first visible image
    first = imglist_jump(
        ctx.current,
        -(ssize_t)(ctx.curr_row * ctx.layout_columns + ctx.curr_col));

    // fix top
    if (!first) {
        first = imglist_first();
        const size_t distance = imglist_distance(first, ctx.current);
        ctx.curr_col = distance % ctx.layout_columns;
        ctx.curr_row = distance / ctx.layout_columns;
    }

    // fix bottom
    if (first != imglist_first() && ctx.layout_rows > 2 &&
        !imglist_jump(first, ctx.layout_columns * (ctx.layout_rows - 2))) {
        const size_t distance = imglist_distance(imglist_first(), ctx.current);
        ctx.curr_col = distance % ctx.layout_columns;
        ctx.curr_row = ctx.layout_rows - 2;
    }
}

/**
 * Select next file.
 * @param direction next image position in list
 * @return true if next image was selected
 */
static bool select_next(enum action_type direction)
{
    struct image* next;
    ssize_t next_col = ctx.curr_col;
    ssize_t next_row = ctx.curr_row;

    loader_stop();
    imglist_lock();

    switch (direction) {
        case action_first_file:
            next = imglist_first();
            next_col = 0;
            next_row = 0;
            break;
        case action_last_file:
            next = imglist_last();
            next_col = (imglist_size() - 1) % ctx.layout_columns;
            next_row = ctx.layout_rows - 1;
            break;
        case action_prev_file:
        case action_step_left:
            next = imglist_prev(ctx.current);
            --next_col;
            break;
        case action_next_file:
        case action_step_right:
            next = imglist_next(ctx.current);
            ++next_col;
            break;
        case action_step_up:
            next = imglist_jump(ctx.current, -(ssize_t)ctx.layout_columns);
            --next_row;
            break;
        case action_step_down:
            next = imglist_jump(ctx.current, ctx.layout_columns);
            ++next_row;
            break;
        case action_page_up:
            next = imglist_jump(ctx.current,
                                -(ssize_t)ctx.layout_columns *
                                    (ctx.layout_rows - 1));
            if (!next) {
                next = imglist_jump(imglist_first(), ctx.curr_col);
            }
            break;
        case action_page_down:
            next = imglist_jump(ctx.current,
                                ctx.layout_columns * (ctx.layout_rows - 1));
            if (!next) {
                next = imglist_last();
                next_col = (imglist_size() - 1) % ctx.layout_columns;
                next_row = ctx.layout_rows - 1;
            }
            break;
        default:
            next = NULL;
            break;
    }

    if (next) {
        // set position of selected image
        if (next_col < 0) {
            next_col = ctx.layout_columns - 1;
            --next_row;
        } else if (next_col >= (ssize_t)ctx.layout_columns) {
            next_col = 0;
            ++next_row;
        }
        if (next_row < 0) {
            next_row = 0;
        } else if (next_row >= (ssize_t)ctx.layout_rows - 1) {
            next_row = ctx.layout_rows - 2;
        }

        ctx.current = next;
        ctx.curr_col = next_col;
        ctx.curr_row = next_row;
        fixup_position();

        info_reset(ctx.current);

        app_redraw();
    }

    imglist_unlock();
    return next;
}

/** Skip current image file. */
static void skip_current(void)
{
    struct image* skip = ctx.current;
    if (select_next(action_next_file) || select_next(action_prev_file)) {
        imglist_lock();
        imglist_remove(skip);
        imglist_unlock();
    } else {
        printf("No more images to view, exit\n");
        app_exit(0);
    }
}

/** Reload. */
static void reload(void)
{
    loader_stop();
    clear_thumbnails(true);
    app_redraw();
}

/**
 * Draw thumbnail.
 * @param window destination window
 * @param column,row position of the thumbnail in layout
 * @param img thumbnail image
 */
static void draw_thumbnail(struct pixmap* window, size_t column, size_t row,
                           const struct image* img)
{
    const struct pixmap* thumb = image_has_thumb(img) ? &img->thumbnail : NULL;
    ssize_t x = column * ctx.thumb_size + ctx.layout_padding * (column + 1);
    ssize_t y = row * ctx.thumb_size + ctx.layout_padding * (row + 1);

    if (img != ctx.current) {
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
        const ssize_t thumb_offset = (thumb_size - ctx.thumb_size) / 2;

        x = max(0, x - thumb_offset);
        y = max(0, y - thumb_offset);
        if (x + thumb_size >= window->width) {
            x = window->width - thumb_size;
        }

        pixmap_fill(window, x, y, thumb_size, thumb_size, ctx.clr_select);

        if (thumb) {
            const size_t thumb_w = thumb->width * THUMB_SELECTED_SCALE;
            const size_t thumb_h = thumb->height * THUMB_SELECTED_SCALE;
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
                const size_t lh = thumb_size - (width - i);
                const argb_t color = base | ARGB_SET_A(alpha - i * alpha_step);
                pixmap_vline(window, lx, ly, lh, color);
            }
            for (size_t i = 0; i < width; ++i) {
                const ssize_t lx = x + width;
                const ssize_t ly = y + thumb_size + i;
                const size_t lw = thumb_size - (width - i) + 1;
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
    struct image* img;

    imglist_lock();

    fixup_position();

    // first visible image
    img = imglist_jump(
        ctx.current,
        -(ssize_t)(ctx.curr_row * ctx.layout_columns + ctx.curr_col));
    assert(img);

    // draw
    for (size_t row = 0; img && row < ctx.layout_rows; ++row) {
        for (size_t col = 0; img && col < ctx.layout_columns; ++col) {
            if (!image_has_thumb(img)) {
                loader_start();
            }
            if (img != ctx.current) {
                draw_thumbnail(window, col, row, img);
            }
            img = imglist_next(img);
        }
    }

    imglist_unlock();

    draw_thumbnail(window, ctx.curr_col, ctx.curr_row, ctx.current);
}

/** Mode handler: window redraw. */
static void on_redraw(struct pixmap* window)
{
    pixmap_fill(window, 0, 0, window->width, window->height, ctx.clr_window);
    draw_thumbnails(window);
    info_print(window);
}

/** Mode handler: window resize. */
static void on_resize(void)
{
    loader_stop();
    update_layout();
}

/** Mode handler: apply action. */
static void on_action(const struct action* action)
{
    switch (action->type) {
        case action_antialiasing:
            ctx.thumb_aa = aa_switch(ctx.thumb_aa, action->params);
            info_update(info_status, "Anti-aliasing: %s",
                        aa_name(ctx.thumb_aa));
            reload();
            break;
        case action_first_file:
        case action_last_file:
        case action_prev_file:
        case action_next_file:
        case action_step_left:
        case action_step_right:
        case action_step_up:
        case action_step_down:
        case action_page_up:
        case action_page_down:
            select_next(action->type);
            break;
        case action_skip_file:
            skip_current();
            break;
        case action_reload:
            reload();
            break;
        default:
            break;
    }
}

/** Mode handler: get currently viewed image. */
static struct image* on_current(void)
{
    return ctx.current;
}

/** Mode handler: activate viewer. */
static void on_activate(struct image* image)
{
    ctx.current = image;
    ctx.curr_col = SIZE_MAX;
    ctx.curr_row = SIZE_MAX;

    if (!image_has_thumb(ctx.current)) {
        image_thumb_create(ctx.current, ctx.thumb_size, ctx.thumb_fill,
                           ctx.thumb_aa);
    }
    image_free(ctx.current, IMGFREE_FRAMES);

    info_reset(ctx.current);
    update_layout();
}

/** Mode handler: deactivate viewer. */
static struct image* on_deactivate(void)
{
    loader_stop();
    return ctx.current;
}

void gallery_init(const struct config* cfg, struct mode_handlers* handlers)
{
    ctx.thumb_size = config_get_num(cfg, CFG_GALLERY, CFG_GLRY_SIZE, 1, 4096);
    ctx.thumb_cache = config_get_num(cfg, CFG_GALLERY, CFG_GLRY_CACHE, 0, 4096);
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
    handlers->resize = on_resize;
    handlers->current = on_current;
    handlers->activate = on_activate;
    handlers->deactivate = on_deactivate;
}

void gallery_destroy(void)
{
    loader_stop();
}
