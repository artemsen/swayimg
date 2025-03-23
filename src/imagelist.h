// SPDX-License-Identifier: MIT
// List of images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "config.h"
#include "image.h"

/**
 * Initialize global image list context.
 * @param cfg config instance
 */
void imglist_init(const struct config* cfg);

/**
 * Destroy global image list context.
 */
void imglist_destroy(void);

/**
 * Add image source to the list.
 * @param source image source to add (file path or special prefix)
 * @return created image entry or NULL on errors or if source is deirectory
 */
struct image* imglist_add(const char* source);

/**
 * Remove image source to the list.
 * @param img image instance to remove
 */
void imglist_remove(struct image* img);

/**
 * Reindex the image list.
 */
void imglist_reindex(void);

/**
 * Get image list size.
 * @return total number of entries in the list
 */
size_t imglist_size(void);

/**
 * Get the first image entry.
 * @return first image instance or NULL list is empty
 */
struct image* imglist_first(void) __attribute__((warn_unused_result));

/**
 * Get the last entry index.
 * @return last image instance or NULL list is empty
 */
struct image* imglist_last(void) __attribute__((warn_unused_result));

/**
 * Get next image entry.
 * @param img start image entry
 * @return next nearest image instance or NULL if `img` is the last entry
 */
struct image* imglist_next(struct image* img)
    __attribute__((warn_unused_result));

/**
 * Get previous image entry.
 * @param img start image entry
 * @return previous nearest image instance or NULL if `img` is the first entry
 */
struct image* imglist_prev(struct image* img)
    __attribute__((warn_unused_result));

/**
 * Get next image entry given the loop setting.
 * @param img start image entry
 * @return next nearest image instance or NULL next instance not found
 */
struct image* imglist_next_file(struct image* img)
    __attribute__((warn_unused_result));

/**
 * Get previous image entry given the loop setting.
 * @param img start image entry
 * @return previous nearest image instance or NULL previous instance not found
 */
struct image* imglist_prev_file(struct image* img)
    __attribute__((warn_unused_result));

/**
 * Get next image entry with different parent (another dir).
 * @param img start image entry
 * @return next nearest image instance or NULL next instance not found
 */
struct image* imglist_next_dir(struct image* img)
    __attribute__((warn_unused_result));

/**
 * Get previous image entry with different parent (another dir).
 * @param img start image entry
 * @return next previous image instance or NULL next instance not found
 */
struct image* imglist_prev_dir(struct image* img)
    __attribute__((warn_unused_result));

/**
 * Get random image entry.
 * @param img image entry to exclude (current one)
 * @return random image entry
 */
struct image* imglist_rand(struct image* img)
    __attribute__((warn_unused_result));

/**
 * Get image entry in specified distance from the start (forward).
 * @param img start position
 * @param distance number entries to skip
 * @return image entry
 */
struct image* imglist_fwd(struct image* img, size_t distance)
    __attribute__((warn_unused_result));

/**
 * Get image entry in specified distance from the start (backward).
 * @param img start position
 * @param distance number entries to skip
 * @return image entry
 */
struct image* imglist_back(struct image* img, size_t distance)
    __attribute__((warn_unused_result));

/**
 * Get distance between two image entries.
 * @param start,end image entries
 * @return number of entries in range [start,end]
 */
size_t imglist_distance(const struct image* start, const struct image* end);

/**
 * File watcher notification callback.
 */
typedef void (*imglist_watch_cb)(void);

/**
 * Add image file to watcher for update/delete.
 * @param img image instance to watch
 * @param callback change notification callback
 */
void imglist_watch(struct image* img, imglist_watch_cb callback);

////////////////////////////////////////////////////////////////////////////////
// DEPRECATED API
#define IMGLIST_INVALID SIZE_MAX
size_t image_list_size(void);
const char* image_list_get(size_t index);
size_t image_list_nearest(size_t start, bool forward, bool loop);
size_t image_list_distance(size_t start, size_t end);
size_t image_list_jump(size_t start, size_t distance, bool forward);
size_t image_list_next_file(size_t start);
size_t image_list_prev_file(size_t start);
size_t image_list_rand_file(size_t exclude);
size_t image_list_next_dir(size_t start);
size_t image_list_prev_dir(size_t start);
size_t image_list_first(void);
size_t image_list_last(void);
size_t image_list_skip(size_t index);
