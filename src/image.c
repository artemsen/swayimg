// SPDX-License-Identifier: MIT
// Image instance: pixel data and meta info.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "image.h"

#include "buildcfg.h"
#include "fs.h"

#ifdef HAVE_LIBPNG
#include "formats/png.h"
#endif // HAVE_LIBPNG

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
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
    }

    return img;
}

bool image_clear(struct image* img, size_t mask)
{
    bool all_free;

    if ((mask & IMGDATA_FRAMES) && image_has_frames(img)) {
        struct imgdec* decoder = &img->data->decoder;
        struct array* frames = img->data->frames;

        if (decoder->data) {
            decoder->free(img->data);
        }
        memset(decoder, 0, sizeof(*decoder));

        for (size_t i = 0; i < frames->size; ++i) {
            struct imgframe* frame = arr_nth(frames, i);
            pixmap_free(&frame->pm);
        }
        arr_free(frames);
        img->data->frames = NULL;
    }

    if ((mask & IMGDATA_THUMB) && image_has_thumb(img)) {
        pixmap_free(&img->data->thumbnail);
        img->data->thumbnail.data = NULL;
    }

    // automatically free if there are no frames or thumbnail
    if (!image_has_frames(img) && !image_has_thumb(img)) {
        mask |= IMGDATA_INFO;
    }
    if ((mask & IMGDATA_INFO) && image_has_info(img)) {
        struct array* info = img->data->info;
        for (size_t i = 0; i < info->size; ++i) {
            struct imginfo* inf = arr_nth(info, i);
            free(inf->key);
        }
        arr_free(img->data->info);
        img->data->info = NULL;
    }

    all_free =
        !image_has_frames(img) && !image_has_thumb(img) && !image_has_info(img);

    if (all_free && img->data) {
        // rest part of data
        img->data->alpha = false;
        free(img->data->parent);
        img->data->parent = NULL;
        free(img->data->format);
        img->data->format = NULL;
    }

    return all_free;
}

void image_free(struct image* img, size_t mask)
{
    if (img->data && image_clear(img, mask)) {
        free(img->data);
        img->data = NULL;
    }
    if (mask == IMGDATA_SELF) {
        free(img);
    }
}

void image_attach(struct image* img, struct image* from)
{
    struct imgdata* src = from->data;
    struct imgdata* dst = img->data;

    assert(strcmp(from->source, img->source) == 0); // same source

    if (!dst) {
        img->data = calloc(1, sizeof(*img->data));
        if (!img->data) {
            return;
        }
        dst = img->data;
    }

    if (src->decoder.data) {
        if (dst->decoder.data) {
            dst->decoder.free(img->data);
        }
        memcpy(&dst->decoder, &src->decoder, sizeof(dst->decoder));
        memset(&src->decoder, 0, sizeof(src->decoder));
    }

    if (src->frames) {
        image_clear(img, IMGDATA_FRAMES);
        dst->frames = src->frames;
        src->frames = NULL;
    }

    if (src->info) {
        image_clear(img, IMGDATA_INFO);
        dst->info = src->info;
        src->info = NULL;
    }

    if (src->thumbnail.data) {
        image_clear(img, IMGDATA_THUMB);
        dst->thumbnail = src->thumbnail;
        src->thumbnail.data = NULL;
    }

    if (src->parent) {
        free(dst->parent);
        dst->parent = src->parent;
        src->parent = NULL;
    }

    if (src->format) {
        free(dst->format);
        dst->format = src->format;
        src->format = NULL;
    }

    dst->alpha = src->alpha;

    // update name
    if (strcmp(img->source, LDRSRC_STDIN) == 0 ||
        strncmp(img->source, LDRSRC_EXEC, LDRSRC_EXEC_LEN) == 0) {
        img->name = img->source;
    } else {
        img->name = fs_name(img->source);
    }
}

bool image_export(const struct image* img, size_t frame, const char* path)
{
#ifdef HAVE_LIBPNG
    struct imgframe* fr = arr_nth(img->data->frames, frame);
    return fr && export_png(&fr->pm, NULL, path);
#else
    (void)img;
    (void)frame;
    (void)path;
    return false;
#endif // HAVE_LIBPNG
}

void image_render(struct image* img, size_t frame, enum aa_mode scaler,
                  double scale, ssize_t x, ssize_t y, struct pixmap* dst)
{
    if (img->data->decoder.render) {
        // image specific renderer
        img->data->decoder.render(img->data, scale, x, y, dst);
    } else {
        // generic software renderer
        const struct imgframe* iframe = arr_nth(img->data->frames, frame);
        assert(iframe);
        if (scale == 1.0) {
            pixmap_copy(&iframe->pm, dst, x, y, img->data->alpha);
        } else {
            software_render(scaler, &iframe->pm, dst, x, y, scale,
                            img->data->alpha);
        }
    }
}

bool image_has_frames(const struct image* img)
{
    return img && img->data && img->data->frames;
}

bool image_has_thumb(const struct image* img)
{
    return img && img->data && img->data->thumbnail.data;
}

bool image_has_info(const struct image* img)
{
    return img && img->data && img->data->info;
}

void image_flip_vertical(struct image* img)
{
    if (img->data->decoder.flip) {
        // image specific flip
        img->data->decoder.flip(img->data, true);
    } else {
        struct array* frames = img->data->frames;
        for (size_t i = 0; i < frames->size; ++i) {
            struct imgframe* frame = arr_nth(frames, i);
            pixmap_flip_vertical(&frame->pm);
        }
    }
}

void image_flip_horizontal(struct image* img)
{
    if (img->data->decoder.flip) {
        // image specific flip
        img->data->decoder.flip(img->data, false);
    } else {
        struct array* frames = img->data->frames;
        for (size_t i = 0; i < frames->size; ++i) {
            struct imgframe* frame = arr_nth(frames, i);
            pixmap_flip_horizontal(&frame->pm);
        }
    }
}

void image_rotate(struct image* img, size_t angle)
{
    assert(angle == 90 || angle == 180 || angle == 270);

    if (img->data->decoder.rotate) {
        // image specific rotate
        img->data->decoder.rotate(img->data, angle);
    } else {
        struct array* frames = img->data->frames;
        for (size_t i = 0; i < frames->size; ++i) {
            struct imgframe* frame = arr_nth(frames, i);
            pixmap_rotate(&frame->pm, angle);
        }
    }
}

bool image_thumb_create(struct image* img, size_t size, bool fill,
                        enum aa_mode aa_mode)
{
    assert(!image_has_thumb(img));

    if (!image_has_frames(img)) {
        return false;
    }

    struct imgframe* frame = arr_nth(img->data->frames, 0);
    const struct pixmap* full = &frame->pm;
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

    if (pixmap_create(&img->data->thumbnail, thumb_width, thumb_height)) {
        image_render(img, 0, aa_mode, scale, offset_x, offset_y,
                     &img->data->thumbnail);
    }

    return image_has_thumb(img);
}

bool image_thumb_load(struct image* img, const char* path)
{
    assert(!image_has_thumb(img));

    struct image* thumb = image_create(path);
    if (thumb) {
        if (image_load(thumb) == imgload_success) {
            if (!img->data) {
                img->data = calloc(1, sizeof(*img->data));
            }
            if (img->data) {
                struct imgframe* frame = arr_nth(thumb->data->frames, 0);
                img->data->alpha = thumb->data->alpha;
                img->data->thumbnail = frame->pm;
                frame->pm.data = NULL;
            }
        }
        image_free(thumb, IMGDATA_SELF);
    }

    return image_has_thumb(img);
}

bool image_thumb_save(const struct image* img, const char* path)
{
#ifdef HAVE_LIBPNG
    assert(image_has_thumb(img));
    return export_png(&img->data->thumbnail, img->data->info, path);
#else
    (void)img;
    (void)path;
    return false;
#endif // HAVE_LIBPNG
}

void image_set_format(struct imgdata* img, const char* fmt, ...)
{
    va_list args;
    int len;

    assert(!img->format);

    va_start(args, fmt);
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
    len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (len > 0) {
        ++len; // last null
        img->format = malloc(len);
        if (img->format) {
            va_start(args, fmt);
            vsprintf(img->format, fmt, args);
            va_end(args);
        }
    }
}

void image_add_info(struct imgdata* img, const char* key, const char* fmt, ...)
{
    va_list args;
    struct array* info;
    struct imginfo entry;
    size_t val_len;
    const size_t key_len = strlen(key) + 1 /* last null */;

    // append new entry to info array
    if (!img->info) {
        img->info = arr_create(0, sizeof(struct imginfo));
        if (!img->info) {
            return;
        }
    }

    // get value string size
    va_start(args, fmt);
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
    val_len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (val_len <= 0) {
        return;
    }
    ++val_len; // last null

    // create new entry
    entry.key = malloc(key_len + val_len);
    if (!entry.key) {
        return;
    }
    memcpy(entry.key, key, key_len);
    entry.value = entry.key + key_len;
    va_start(args, fmt);
    vsprintf(entry.value, fmt, args);
    va_end(args);

    info = arr_append(img->info, &entry, 1);
    if (info) {
        img->info = info;
    } else {
        free(entry.key);
    }
}

struct array* image_alloc_frames(struct imgdata* img, size_t num)
{
    assert(!img->frames);
    img->frames = arr_create(num, sizeof(struct imgframe));
    return img->frames;
}

struct pixmap* image_alloc_frame(struct imgdata* img, size_t width,
                                 size_t height)
{
    struct pixmap* pm = NULL;
    struct array* frames;

    frames = image_alloc_frames(img, 1);
    if (frames) {
        pm = arr_nth(frames, 0);
        if (!pixmap_create(pm, width, height)) {
            return NULL;
        }
    }

    return pm;
}
