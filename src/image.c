// SPDX-License-Identifier: MIT
// Image instance: pixel data and meta info.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "image.h"

#include "array.h"
#include "buildcfg.h"
#ifdef HAVE_LIBPNG
#include "formats/png.h"
#endif // HAVE_LIBPNG

#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct image* image_create(const char* source)
{
    struct image* img;
    const size_t len = strlen(source) + 1 /* last null */;

    img = calloc(1, sizeof(struct image) + len);
    if (img) {
        img->source = (char*)img + sizeof(struct image);
        memcpy(img->source, source, len);
        image_addref(img);
    }

    return img;
}

void image_addref(struct image* img)
{
    __sync_add_and_fetch(&img->ref_count, 1);
}

void image_deref(struct image* img)
{
    assert(img && img->ref_count > 0);

    if (__sync_sub_and_fetch(&img->ref_count, 1) == 0) {
        image_thumb_free(img);
        image_unload(img);
        free(img);
    }
}

void image_unload(struct image* img)
{
    // free frames
    for (size_t i = 0; i < img->num_frames; ++i) {
        pixmap_free(&img->frames[i].pm);
    }
    free(img->frames);
    img->frames = NULL;
    img->num_frames = 0;

    // free descriptions
    free(img->parent_dir);
    img->parent_dir = NULL;
    free(img->format);
    img->format = NULL;
    img->name = NULL;

    // free meta data
    list_for_each(img->info, struct image_info, it) {
        free(it);
    }
    img->info = NULL;
}

void image_flip_vertical(struct image* img)
{
    for (size_t i = 0; i < img->num_frames; ++i) {
        pixmap_flip_vertical(&img->frames[i].pm);
    }
}

void image_flip_horizontal(struct image* img)
{
    for (size_t i = 0; i < img->num_frames; ++i) {
        pixmap_flip_horizontal(&img->frames[i].pm);
    }
}

void image_rotate(struct image* img, size_t angle)
{
    assert(angle == 90 || angle == 180 || angle == 270);
    for (size_t i = 0; i < img->num_frames; ++i) {
        pixmap_rotate(&img->frames[i].pm, angle);
    }
}

void image_thumb_create(struct image* img, size_t size, bool fill,
                        enum aa_mode aa_mode)
{
    assert(!img->thumbnail.data);
    assert(img->num_frames);

    const struct pixmap* full = &img->frames[0].pm;
    const size_t real_width = full->width;
    const size_t real_height = full->height;

    const float scale_width = 1.0 / ((float)real_width / size);
    const float scale_height = 1.0 / ((float)real_height / size);
    const float scale =
        fill ? max(scale_width, scale_height) : min(scale_width, scale_height);

    size_t thumb_width = scale * real_width;
    size_t thumb_height = scale * real_height;
    ssize_t offset_x, offset_y;

    if (fill) {
        offset_x = size / 2 - thumb_width / 2;
        offset_y = size / 2 - thumb_height / 2;
        thumb_width = size;
        thumb_height = size;
    } else {
        offset_x = 0;
        offset_y = 0;
    }

    if (pixmap_create(&img->thumbnail, thumb_width, thumb_height)) {
        pixmap_scale(aa_mode, full, &img->thumbnail, offset_x, offset_y, scale,
                     img->alpha);
    }
}

void image_thumb_free(struct image* img)
{
    pixmap_free(&img->thumbnail);
}

bool image_thumb_load(struct image* img, const char* path)
{
    assert(!img->thumbnail.data);

    struct image* thumb = image_create(path);
    if (!thumb) {
        return false;
    }
    if (image_load(thumb) == ldr_success) {
        img->thumbnail = thumb->frames[0].pm;
        thumb->frames[0].pm.data = NULL;
    }
    image_deref(thumb);

    return !!img->thumbnail.data;
}

bool image_thumb_save(const struct image* img, const char* path)
{
#ifdef HAVE_LIBPNG
    assert(img->thumbnail.data);
    return export_png(&img->thumbnail, img->info, path);
#else
    (void)img;
    (void)path;
    return false;
#endif // HAVE_LIBPNG
}
