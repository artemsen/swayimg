// SPDX-License-Identifier: MIT
// Gallery mode.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "gallery.h"

#include "application.h"
#include "array.h"
#include "fs.h"
#include "imglist.h"
#include "info.h"
#include "layout.h"
#include "tpool.h"
#include "ui/ui.h"

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Limits for thumbnail size
#define THIMB_SIZE_MIN 50
#define THIMB_SIZE_MAX 1000

static const char* aspect_names[] = {
    [aspect_fit] = "fit",
    [aspect_fill] = "fill",
    [aspect_keep] = "keep",
};

/** Gallery context. */
struct gallery {
    size_t cache; ///< Max number of thumbnails in cache
    bool preload; ///< Preload invisible thumbnails

    bool thumb_aa_en;      ///< Enable/disable anti-aliasing mode
    enum aa_mode thumb_aa; ///< Anti-aliasing mode

    enum thumb_aspect aspect; ///< Thumbnail aspect ratio (fit/fill/keep)
    bool thumb_pstore;        ///< Use persistent storage for thumbnails

    argb_t clr_window;     ///< Window background
    argb_t clr_background; ///< Tile background
    argb_t clr_select;     ///< Selected tile background
    argb_t clr_border;     ///< Selected tile border
    size_t border_width;   ///< Selected tile border size
    float selected_scale;  ///< Selected tile scale

    struct layout layout; ///< Thumbnail layout

    struct keybind* kb; ///< Key bindings
};

/** Global gallery context. */
static struct gallery ctx;

/**
 * Get path for the thumbnail on persistent storage.
 * @param source original image source
 * @param path output buffer
 * @param path_max size of the buffer
 * @return length of the result path without last null
 */
static size_t pstore_path(const char* source, char* path, size_t path_max)
{
    char postfix[16];
    int postfix_len;
    size_t len;

    if (strcmp(source, LDRSRC_STDIN) == 0 ||
        strncmp(source, LDRSRC_EXEC, LDRSRC_EXEC_LEN) == 0) {
        return 0;
    }

    // get directory to store thumbnails
    len = fs_envpath("XDG_CACHE_HOME", "/swayimg", path, path_max);
    if (!len) {
        len = fs_envpath("HOME", "/.cache/swayimg", path, path_max);
    }
    if (!len) {
        return 0;
    }

    // append file name
    len = fs_append_path(source, path, path_max);
    if (!len) {
        return 0;
    }

    // append postfix
    postfix_len = snprintf(postfix, sizeof(postfix), ".%04x%d%d",
                           (uint16_t)ctx.layout.thumb_size, ctx.aspect,
                           ctx.thumb_aa_en ? ctx.thumb_aa : aa_nearest);
    if (postfix_len <= 0 || len + postfix_len + 1 >= path_max) {
        return 0;
    }

    memcpy(path + len, postfix, postfix_len + 1);
    len += postfix_len;

    return len;
}

/**
 * Save thumbnail on persistent storage.
 * @param img image with thumbnail to save
 */
void pstore_save(const struct image* img)
{
    char path[PATH_MAX] = { 0 };
    char* tmp;

    if (!image_thumb_get(img)) {
        return;
    }

    if (!pstore_path(img->source, path, sizeof(path))) {
        return;
    }

    // export thumbnail to file, exporting to png is slow so we use a temporary
    // file to prevent incomplete export from being used in other threads
    tmp = str_dup(path, NULL);
    if (tmp && str_append(".tmp", 0, &tmp)) {
        image_thumb_save(img, tmp);
        rename(tmp, path);
    } else {
        image_thumb_save(img, path);
    }
    free(tmp);
}

/**
 * Load thumbnail from persistent storage.
 * @param img image for loading thumbnail
 * @return true if thumbnail loaded
 */
bool pstore_load(struct image* img)
{
    char path[PATH_MAX] = { 0 };
    struct stat st_image;
    struct stat st_thumb;

    if (!pstore_path(img->source, path, sizeof(path))) {
        return false;
    }

    // check modification time
    if (stat(img->source, &st_image) == -1 || stat(path, &st_thumb) == -1 ||
        st_image.st_mtim.tv_sec > st_thumb.st_mtim.tv_sec) {
        return false;
    }

    return image_thumb_load(img, path);
}

/**
 * Remove non-visible thumbnails to save memory.
 * @param all remove even visible thumbnails
 */
static void clear_thumbnails(bool all)
{
    imglist_lock();

    if (all) {
        struct image* img = imglist_first();
        while (img) {
            image_free(img, IMGDATA_THUMB);
            img = imglist_next(img, false);
        }
    } else if (ctx.cache) {
        layout_update(&ctx.layout);
        layout_clear(&ctx.layout, ctx.cache);
    }

    imglist_unlock();
}

/**
 * Skip current image file.
 * @param remove flag to remove current image from the image list
 */
static void skip_current(bool remove)
{
    struct image* skip = ctx.layout.current;

    if (layout_select(&ctx.layout, layout_right) ||
        layout_select(&ctx.layout, layout_left)) {
        if (remove) {
            imglist_remove(skip);
        }
        ui_set_title(ctx.layout.current->name);
        app_redraw();
    } else {
        printf("No more images to view, exit\n");
        app_exit(0);
    }
}

/** Thumbnail cleaner as a task in thread pool. */
static void thumb_clear(__attribute__((unused)) void* data)
{
    clear_thumbnails(false);
}

/** Thumbnail loader as a task in thread pool. */
static void thumb_load(void* data)
{
    struct image* img = data;
    struct image* origin;

    // check if thumbnail already loaded by another thread
    imglist_lock();
    origin = imglist_find(img->source);
    if (!origin ||                 // removed
        image_thumb_get(origin) || // already loaded
        image_thumb_create(origin, ctx.layout.thumb_size, ctx.aspect,
                           ctx.thumb_aa_en ? ctx.thumb_aa : aa_nearest)) {
        imglist_unlock();
        app_redraw();
        return;
    }
    imglist_unlock();

    // try to load from persistent storage
    if (ctx.thumb_pstore) {
        pstore_load(img);
    }
    // load entire image and convert it to thumbnail
    if (!image_thumb_get(img) && image_load(img) == imgload_success) {
        if (image_thumb_create(img, ctx.layout.thumb_size, ctx.aspect,
                               ctx.thumb_aa_en ? ctx.thumb_aa : aa_nearest)) {
            if (ctx.thumb_pstore) {
                // save to thumbnail to persistent storage
                struct imgframe* frame = arr_nth(img->data->frames, 0);
                if (frame->pm.width > ctx.layout.thumb_size &&
                    frame->pm.height > ctx.layout.thumb_size) {
                    pstore_save(img);
                }
            }
        }
        image_free(img, IMGDATA_FRAMES); // not needed anymore
    }

    // put thumbnail to image list
    imglist_lock();
    origin = imglist_find(img->source);
    if (origin) {
        if (image_thumb_get(img)) {
            image_attach(origin, img);
        } else {
            // failed to load
            if (origin == ctx.layout.current) {
                skip_current(true);
            } else {
                imglist_remove(origin);
            }
        }
    }
    imglist_unlock();

    app_redraw();
}

/** Image instance deleter for task in thread pool. */
static void thumb_free(void* data)
{
    image_free(data, IMGDATA_SELF);
}

/** Recreate thumnail load queue. */
static void thumb_requeue(void)
{
    struct image* queue;

    assert(imglist_is_locked());

    tpool_cancel();

    queue = layout_ldqueue(&ctx.layout, ctx.preload ? ctx.cache : 0);
    if (queue) {
        list_for_each(queue, struct image, it) {
            tpool_add_task(thumb_load, thumb_free, it);
        }
    }

    // add last task to limit stored thumbnails
    tpool_add_task(thumb_clear, NULL, NULL);
}

/**
 * Select next file.
 * @param direction next image position in list
 * @return true if next image was selected
 */
static bool select_next(enum action_type direction)
{
    bool rc = false;
    enum layout_dir dir;

    switch (direction) {
        case action_first_file:
            dir = layout_first;
            break;
        case action_last_file:
            dir = layout_last;
            break;
        case action_prev_file:
        case action_step_left:
            dir = layout_left;
            break;
        case action_next_file:
        case action_step_right:
            dir = layout_right;
            break;
        case action_step_up:
            dir = layout_up;
            break;
        case action_step_down:
            dir = layout_down;
            break;
        case action_page_up:
            dir = layout_pgup;
            break;
        case action_page_down:
            dir = layout_pgdown;
            break;
        default:
            assert(false && "unreachable code");
            return false;
    }

    imglist_lock();
    rc = layout_select(&ctx.layout, dir);
    if (rc) {
        thumb_requeue();
    }
    imglist_unlock();

    if (rc) {
        info_reset(ctx.layout.current);
    }
    ui_set_title(ctx.layout.current->name);
    info_update_index(info_index, ctx.layout.current->index, imglist_size());
    app_redraw();

    return rc;
}

/** Reload. */
static void reload(void)
{
    tpool_cancel();
    tpool_wait();
    clear_thumbnails(true);
    app_redraw();
}

/**
 * Switch antialiasing mode: handle "antialiasing" action.
 * @param params action parameter
 */
static void switch_antialiasing(const char* params)
{
    if (*params) {
        if (aa_from_name(params, &ctx.thumb_aa)) {
            info_update(info_status, "Anti-aliasing: %s", params);
        } else {
            info_update(info_status, "Invalid anti-aliasing: %s", params);
        }
    } else {
        ctx.thumb_aa_en = !ctx.thumb_aa_en;
        info_update(info_status, "Anti-aliasing: %s",
                    ctx.thumb_aa_en ? "ON" : "OFF");
    }
    reload();
}

/**
 * Set thumbnail size: handle "thumb" action.
 * @param params zoom action parameter
 */
static void thumb_resize(const char* params)
{
    bool valid;
    ssize_t size = ctx.layout.thumb_size;

    if (!params || !*params) {
        valid = false;
    } else if (params[0] == '+' || params[0] == '-') {
        // relative
        ssize_t delta;
        valid = str_to_num(params, 0, &delta, 0);
        if (valid) {
            size += delta;
        }
    } else {
        // absolute
        valid = str_to_num(params, 0, &size, 0);
    }

    if (!valid) {
        info_update(info_status, "Invalid thumb resize operation: %s", params);
        app_redraw();
        return;
    }

    if (size < THIMB_SIZE_MIN) {
        size = THIMB_SIZE_MIN;
    } else if (size > THIMB_SIZE_MAX) {
        size = THIMB_SIZE_MAX;
    }
    if (size != (ssize_t)ctx.layout.thumb_size) {
        imglist_lock();
        ctx.layout.thumb_size = size;
        layout_resize(&ctx.layout, ui_get_width(), ui_get_height());
        imglist_unlock();
        reload();
    }
}

/**
 * Draw thumbnail.
 * @param window destination window
 * @param thumb thumbnail description
 */
static void draw_thumbnail(struct pixmap* window,
                           const struct layout_thumb* lth)
{
    const struct pixmap* pm = image_thumb_get(lth->img);
    const ssize_t x = lth->x;
    const ssize_t y = lth->y;

    if (lth != layout_current(&ctx.layout)) {
        ssize_t thumb_x = x;
        ssize_t thumb_y = y;

        ssize_t bg_x = x;
        ssize_t bg_y = y;
        size_t bg_w = ctx.layout.thumb_size;
        size_t bg_h = ctx.layout.thumb_size;

        if (pm) {
            thumb_x += ctx.layout.thumb_size / 2 - pm->width / 2;
            thumb_y += ctx.layout.thumb_size / 2 - pm->height / 2;

            if (ctx.aspect == aspect_keep) {
                bg_x = thumb_x;
                bg_y = thumb_y;
                bg_w = pm->width;
                bg_h = pm->height;
            }
        }

        pixmap_fill(window, bg_x, bg_y, bg_w, bg_h, ctx.clr_background);
        if (pm) {
            pixmap_copy(pm, window, thumb_x, thumb_y);
        }
    } else {
        // currently selected item
        const size_t thumb_size = ctx.selected_scale * ctx.layout.thumb_size;
        const ssize_t thumb_offset =
            ((ssize_t)thumb_size - (ssize_t)ctx.layout.thumb_size) / 2;

        ssize_t thumb_x = max(0, x - thumb_offset);
        ssize_t thumb_y = max(0, y - thumb_offset);
        size_t thumb_w = thumb_size;
        size_t thumb_h = thumb_size;

        ssize_t bg_x = thumb_x;
        ssize_t bg_y = thumb_y;
        size_t bg_w = thumb_w;
        size_t bg_h = thumb_h;

        if (pm) {
            thumb_w = pm->width * ctx.selected_scale;
            thumb_h = pm->height * ctx.selected_scale;
            thumb_x += thumb_size / 2 - thumb_w / 2;
            thumb_y += thumb_size / 2 - thumb_h / 2;

            if (ctx.aspect == aspect_keep) {
                bg_x = thumb_x;
                bg_y = thumb_y;
                bg_w = thumb_w;
                bg_h = thumb_h;
            }
        }
        pixmap_fill(window, bg_x, bg_y, bg_w, bg_h, ctx.clr_select);

        if (pm) {
            software_render(pm, window, thumb_x, thumb_y, ctx.selected_scale,
                            ctx.thumb_aa_en ? ctx.thumb_aa : aa_nearest, false);
        }

        // border
        if (ARGB_GET_A(ctx.clr_border) && ctx.border_width > 0) {
            pixmap_rect(window, bg_x, bg_y, bg_w, bg_h, ctx.border_width,
                        ctx.clr_border);
        }
    }
}

/**
 * Draw thumbnails.
 * @param window destination window
 */
static void draw_thumbnails(struct pixmap* window)
{
    bool all_loaded = true;

    imglist_lock();
    layout_update(&ctx.layout);

    // draw all exclude the currently selected
    for (size_t i = 0; i < ctx.layout.thumb_total; ++i) {
        const struct layout_thumb* thumb = &ctx.layout.thumbs[i];
        all_loaded &= image_thumb_get(thumb->img) != NULL;
        if (thumb != layout_current(&ctx.layout)) {
            draw_thumbnail(window, thumb);
        }
    }
    // draw only currently selected
    draw_thumbnail(window, layout_current(&ctx.layout));

    if (!all_loaded) {
        thumb_requeue();
    }

    imglist_unlock();
}

/** Redraw window. */
static void redraw(void)
{
    struct pixmap* wnd = ui_draw_begin();
    if (wnd) {
        pixmap_fill(wnd, 0, 0, wnd->width, wnd->height, ctx.clr_window);
        draw_thumbnails(wnd);

        info_update_index(info_index, ctx.layout.current->index,
                          imglist_size());
        info_print(wnd);

        ui_draw_commit();
    }
}

/** Mode handler: window resize. */
static void on_resize(void)
{
    tpool_cancel();

    imglist_lock();
    layout_resize(&ctx.layout, ui_get_width(), ui_get_height());
    imglist_unlock();
}

/** Mode handler: apply action. */
static bool handle_action(const struct action* action)
{
    switch (action->type) {
        case action_antialiasing:
            switch_antialiasing(action->params);
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
            imglist_lock();
            skip_current(true);
            imglist_unlock();
            break;
        case action_redraw:
            redraw();
            break;
        case action_reload:
            reload();
            break;
        case action_thumb:
            thumb_resize(action->params);
            break;
        default:
            return false;
    }
    return true;
}

/** Mode handler: mouse move. */
static void on_mouse_move(__attribute__((unused)) uint8_t mods,
                          __attribute__((unused)) uint32_t btn, size_t x,
                          size_t y, __attribute__((unused)) ssize_t dx,
                          __attribute__((unused)) ssize_t dy)
{
    if (layout_select_at(&ctx.layout, x, y)) {
        imglist_lock();
        layout_update(&ctx.layout);
        thumb_requeue();
        imglist_unlock();
        info_reset(ctx.layout.current);
        ui_set_title(ctx.layout.current->name);
        info_update_index(info_index, ctx.layout.current->index,
                          imglist_size());
        app_redraw();
    }
}

/** Mode handler: mouse click/scroll. */
static bool on_mouse_click(uint8_t mods, uint32_t btn, size_t x, size_t y)
{
    const struct keybind* kb = keybind_find(ctx.kb, MOUSE_TO_XKB(btn), mods);
    if (kb && kb->actions->type == action_mode) {
        if (layout_get_at(&ctx.layout, x, y)) {
            app_switch_mode(kb->actions->params);
        }
        return true;
    }
    return false;
}

/** Mode handler: image list update. */
static void on_imglist(struct image* image, enum fsevent event)
{
    switch (event) {
        case fsevent_create:
            break;
        case fsevent_modify:
            if (image == ctx.layout.current) {
                reload();
            }
            break;
        case fsevent_remove:
            if (image == ctx.layout.current) {
                skip_current(false);
            }
            break;
    }
    app_redraw();
}

/** Mode handler: get currently viewed image. */
static struct image* get_current(void)
{
    return ctx.layout.current;
}

/** Mode handler: get key bindings. */
static struct keybind* get_keybinds(void)
{
    return ctx.kb;
}

/** Mode handler: activate viewer. */
static void on_activate(struct image* image)
{
    imglist_lock();

    ctx.layout.current = image;
    layout_resize(&ctx.layout, ui_get_width(), ui_get_height());

    if (!image_thumb_get(image)) {
        image_thumb_create(image, ctx.layout.thumb_size, ctx.aspect,
                           ctx.thumb_aa_en ? ctx.thumb_aa : aa_nearest);
    }

    imglist_unlock();

    info_reset(image);
    info_update_index(info_index, ctx.layout.current->index, imglist_size());
    ui_set_title(ctx.layout.current->name);
    ui_set_ctype(false);
}

/** Mode handler: deactivate viewer. */
static void on_deactivate(void)
{
    tpool_cancel();
    tpool_wait();
}

void gallery_init(const struct config* cfg, struct mode* handlers)
{
    const struct config* section = config_section(cfg, CFG_GALLERY);

    const size_t ts = config_get_num(section, CFG_GLRY_SIZE, 1, 4096);
    const size_t ps = config_get_num(section, CFG_GLRY_PADDING, 0, 256);
    layout_init(&ctx.layout, ts, ps);

    ctx.cache = config_get_num(section, CFG_GLRY_CACHE, 0, SSIZE_MAX);
    ctx.preload = config_get_bool(section, CFG_GLRY_PRELOAD);

    ctx.thumb_aa_en = true;
    ctx.thumb_aa = aa_mks13;
    if (!aa_from_name(config_get(section, CFG_GLRY_AA), &ctx.thumb_aa)) {
        const char* def = config_get_default(section->name, CFG_GLRY_AA);
        aa_from_name(def, &ctx.thumb_aa);
        config_error_val(section->name, CFG_GLRY_AA);
    }

    ctx.aspect = config_get_oneof(section, CFG_GLRY_ASPECT, aspect_names,
                                  ARRAY_SIZE(aspect_names));
    ctx.thumb_pstore = config_get_bool(section, CFG_GLRY_PSTORE);

    ctx.clr_window = config_get_color(section, CFG_GLRY_WINDOW);
    ctx.clr_background = config_get_color(section, CFG_GLRY_BKG);
    ctx.clr_select = config_get_color(section, CFG_GLRY_SELECT);
    ctx.clr_border = config_get_color(section, CFG_GLRY_BORDER);
    ctx.border_width = config_get_num(section, CFG_GLRY_BORDER_WIDTH, 0, 256);
    ctx.selected_scale =
        config_get_float(section, CFG_GLRY_SELECTED_SCALE, 0.1f, 10.0f);

    // load key bindings
    ctx.kb = keybind_load(config_section(cfg, CFG_KEYS_GALLERY));

    handlers->on_activate = on_activate;
    handlers->on_deactivate = on_deactivate;
    handlers->on_resize = on_resize;
    handlers->on_mouse_move = on_mouse_move;
    handlers->on_mouse_click = on_mouse_click;
    handlers->on_imglist = on_imglist;
    handlers->handle_action = handle_action;
    handlers->get_current = get_current;
    handlers->get_keybinds = get_keybinds;
}

void gallery_destroy(void)
{
    keybind_free(ctx.kb);
}
