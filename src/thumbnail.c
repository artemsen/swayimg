// SPDX-License-Identifier: MIT
// Create/load/store thumbnails.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "thumbnail.h"

#include "imagelist.h"

#include <stdlib.h>

/** Thumbnail context. */
struct thumbnail_context {
    size_t size;                 ///< Size of thumbnail
    bool fill;                   ///< Scale mode (fill/fit)
    enum pixmap_aa_mode aa_mode; ///< Anti-aliasing mode
    struct thumbnail* thumbs;    ///< List of thumbnails
};

/** Global thumbnail context. */
static struct thumbnail_context ctx;

void thumbnail_init(const struct config* cfg)
{
    ctx.size = config_get_num(cfg, CFG_GALLERY, CFG_GLRY_SIZE, 1, 1024);
    ctx.fill = config_get_bool(cfg, CFG_GALLERY, CFG_GLRY_FILL);
    ctx.aa_mode =
        config_get_oneof(cfg, CFG_GALLERY, CFG_GLRY_AA, pixmap_aa_names,
                         ARRAY_SIZE(pixmap_aa_names));
}

void thumbnail_free(void)
{
    list_for_each(ctx.thumbs, struct thumbnail, it) {
        image_free(it->image);
        free(it);
    }
    ctx.thumbs = NULL;
}

enum pixmap_aa_mode thumbnail_get_aa(void)
{
    return ctx.aa_mode;
}

enum pixmap_aa_mode thumbnail_switch_aa(void)
{
    if (++ctx.aa_mode >= ARRAY_SIZE(pixmap_aa_names)) {
        ctx.aa_mode = 0;
    }
    return ctx.aa_mode;
}

void thumbnail_add(struct image* image)
{
    struct thumbnail* entry;
    struct pixmap thumb;
    struct image_frame* frame;
    const struct pixmap* full = &image->frames[0].pm;
    const float scale_width = 1.0 / ((float)full->width / ctx.size);
    const float scale_height = 1.0 / ((float)full->height / ctx.size);
    const float scale = ctx.fill ? max(scale_width, scale_height)
                                 : min(scale_width, scale_height);
    size_t thumb_width = scale * full->width;
    size_t thumb_height = scale * full->height;
    ssize_t offset_x, offset_y;

    if (ctx.fill) {
        offset_x = ctx.size / 2 - thumb_width / 2;
        offset_y = ctx.size / 2 - thumb_height / 2;
        thumb_width = ctx.size;
        thumb_height = ctx.size;
    } else {
        offset_x = 0;
        offset_y = 0;
    }

    // create new entry
    entry = malloc(sizeof(*entry));
    if (!entry) {
        image_free(image);
        return;
    }
    entry->image = image;
    entry->width = image->frames[0].pm.width;
    entry->height = image->frames[0].pm.height;

    // create thumbnail from image (replace the first frame)
    if (!pixmap_create(&thumb, thumb_width, thumb_height)) {
        free(entry);
        image_free(image);
        return;
    }
    pixmap_scale(ctx.aa_mode, full, &thumb, offset_x, offset_y, scale,
                 image->alpha);
    image_free_frames(image);
    frame = image_create_frames(image, 1);
    if (!frame) {
        free(entry);
        pixmap_free(&thumb);
        image_free(image);
        return;
    }
    frame->pm = thumb;

    // add thumbnail to the list
    ctx.thumbs = list_append(ctx.thumbs, entry);
}

const struct thumbnail* thumbnail_get(size_t index)
{
    list_for_each(ctx.thumbs, struct thumbnail, it) {
        if (it->image->index == index) {
            return it;
        }
    }
    return NULL;
}

void thumbnail_remove(size_t index)
{
    list_for_each(ctx.thumbs, struct thumbnail, it) {
        if (it->image->index == index) {
            ctx.thumbs = list_remove(it);
            image_free(it->image);
            free(it);
            break;
        }
    }
}

void thumbnail_clear(size_t min_id, size_t max_id)
{
    if (min_id == IMGLIST_INVALID && max_id == IMGLIST_INVALID) {
        list_for_each(ctx.thumbs, struct thumbnail, it) {
            ctx.thumbs = list_remove(it);
            image_free(it->image);
            free(it);
        }
    } else {
        list_for_each(ctx.thumbs, struct thumbnail, it) {
            if ((min_id != IMGLIST_INVALID && it->image->index < min_id) ||
                (max_id != IMGLIST_INVALID && it->image->index > max_id)) {
                ctx.thumbs = list_remove(it);
                image_free(it->image);
                free(it);
            }
        }
    }
}

