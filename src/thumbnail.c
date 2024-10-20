// SPDX-License-Identifier: MIT
// Image instance: pixel data and meta info.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>
// Copyright (C) 2024 Rentib <sbitner420@tutanota.com>

#include "image.h"
#include "pixmap.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#if defined(__linux__)
#include <linux/limits.h>
#elif defined(__OpenBSD__) || defined(__FreeBSD__)
#define PATH_MAX 4096
#endif

/* TODO: move to config */
static bool thumbnails_disk_cache = true;

/**
 * Write thumbnail to path.
 * @param thumb pixmap context
 * @param path path to save to
 * @return true pixmap was saved
 */
bool thumbnail_save(struct pixmap* thumb, const char* path)
{
    FILE* fp;
    uint32_t i;

    if (!(fp = fopen(path, "wb"))) {
        return false;
    }

    // TODO: add alpha channel
    fprintf(fp, "P6\n%zu %zu\n255\n", thumb->width, thumb->height);
    for (i = 0; i < thumb->width * thumb->height; ++i) {
        uint8_t color[] = { (((thumb->data[i] >> (8 * 2)) & 0xff)),
                            (((thumb->data[i] >> (8 * 1)) & 0xff)),
                            (((thumb->data[i] >> (8 * 0)) & 0xff)) };
        fwrite(color, 3, 1, fp);
    }

    fclose(fp);
    return true;
}

/**
 * Allocate/reallocate pixel map.
 * @param thumb pixmap context
 * @param path path to load from
 * @return true pixmap was loaded
 */
bool thumbnail_load(struct pixmap* thumb, const char* path)
{
    FILE* fp;
    uint32_t i;

    if (!(fp = fopen(path, "rb"))) {
        return false;
    }

    char header[3];
    if (fscanf(fp, "%2s\n%zu %zu\n255\n", header, &thumb->width,
               &thumb->height) != 3) {
        goto fail;
    }

    if (strcmp(header, "P6")) {
        goto fail;
    }

    /* NOTE: It might be that we want the pixmap to have exact width and height
     * and return false if it doesn't match */
    if (!pixmap_create(thumb, thumb->width, thumb->height)) {
        goto fail;
    }

    for (i = 0; i < thumb->width * thumb->height; ++i) {
        uint8_t color[3];
        if (fread(color, 3, 1, fp) != 1) {
            pixmap_free(thumb);
            goto fail;
        }
        thumb->data[i] = ARGB(0xff, color[0], color[1], color[2]);
    }

    fclose(fp);
    return true;

fail:
    fclose(fp);
    return false;
}

// TODO: probably should be moved to config, to make use of expand_path()
/**
 * Get path to cached image thumbnail.
 * @param image image
 * @param path array in which the path should be stored
 * @return true if successful
 */
static bool get_image_thumb_path(const struct image* image, char* path)
{
    static char* cache_dir = NULL;
    int r;
    if (!cache_dir) {
        cache_dir = getenv("XDG_CACHE_HOME");
        if (cache_dir) {
            cache_dir = strcat(cache_dir, "/swayimg");
        } else {
            cache_dir = getenv("HOME");
            if (!cache_dir) {
                return false;
            }
            cache_dir = strcat(cache_dir, "/.swayimg");
        }
    }
    r = snprintf(path, PATH_MAX, "%s%s", cache_dir, image->source);
    return r >= 0 && r < PATH_MAX;
}

// TODO: where should this function be?
/**
 * Makes directories like `mkdir -p`.
 * @param path absolute path to the directory to be created
 * @return true if successful
 */
static bool make_directories(char* path)
{
    char* slash;

    if (!path || !*path) {
        return false;
    }

    slash = path;
    while (true) {
        slash = strchr(slash + 1, '/');
        if (!slash) {
            break;
        }
        *slash = '\0';
        if (mkdir(path, 0755) && errno != EEXIST) {
            // TODO: do we want to print error message?
            return false;
        }
        *slash = '/';
    }

    return true;
}

bool thumbnail_create(struct pixmap* thumb, const struct image* image,
                      size_t size, bool fill, bool antialias)
{
    const struct pixmap* full = &image->frames[0].pm;
    const float scale_width = 1.0 / ((float)full->width / size);
    const float scale_height = 1.0 / ((float)full->height / size);
    const float scale =
        fill ? max(scale_width, scale_height) : min(scale_width, scale_height);
    size_t thumb_width = scale * full->width;
    size_t thumb_height = scale * full->height;
    ssize_t offset_x, offset_y;
    enum pixmap_scale scaler;
    char thumb_path[PATH_MAX] = { 0 };
    struct stat attr_img, attr_thumb;

    // get thumbnail from cache
    if (thumbnails_disk_cache && get_image_thumb_path(image, thumb_path) &&
        !stat(image->source, &attr_img) && !stat(thumb_path, &attr_thumb) &&
        difftime(attr_img.st_ctime, attr_thumb.st_ctime) <= 0 &&
        thumbnail_load(thumb, thumb_path)) {
        return true;
    }

    if (antialias) {
        scaler = (scale > 1.0) ? pixmap_bicubic : pixmap_average;
    } else {
        scaler = pixmap_nearest;
    }

    if (fill) {
        offset_x = size / 2 - thumb_width / 2;
        offset_y = size / 2 - thumb_height / 2;
        thumb_width = size;
        thumb_height = size;
    } else {
        offset_x = 0;
        offset_y = 0;
    }

    // create thumbnail
    if (!pixmap_create(thumb, thumb_width, thumb_height)) {
        return false;
    }
    pixmap_scale(scaler, full, thumb, offset_x, offset_y, scale, image->alpha);

    // save thumbnail to disk
    if (thumbnails_disk_cache && make_directories(thumb_path)) {
        thumbnail_save(thumb, thumb_path);
    }

    return true;
}
