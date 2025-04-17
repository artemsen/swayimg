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
 * Lock the list with mutex.
 */
void imglist_lock(void);

/**
 * Unlock the list.
 */
void imglist_unlock(void);

/**
 * Check if image list is locked.
 */
bool imglist_is_locked(void);

/**
 * Load image list from specified sources.
 * @param sources array of sources
 * @param num number of sources in the array
 * @return first image instance to show or NULL if list is empty
 */
struct image* imglist_load(const char* const* sources, size_t num);

/**
 * Remove image source to the list.
 * @param img image instance to remove
 */
void imglist_remove(struct image* img);

/**
 * Find image instance by source path.
 * @param source image source to search
 * @return image entry or NULL if source not found
 */
struct image* imglist_find(const char* source);

/**
 * Get image list size.
 * @return total number of entries in the list
 */
size_t imglist_size(void);

/**
 * Get the first image entry.
 * @return first image instance or NULL list is empty
 */
struct image* imglist_first(void);

/**
 * Get the last entry index.
 * @return last image instance or NULL list is empty
 */
struct image* imglist_last(void);

/**
 * Get next image entry.
 * @param img start image entry
 * @return next nearest image instance or NULL if `img` is the last entry
 */
struct image* imglist_next(struct image* img);

/**
 * Get previous image entry.
 * @param img start image entry
 * @return previous nearest image instance or NULL if `img` is the first entry
 */
struct image* imglist_prev(struct image* img);

/**
 * Get next image entry given the loop setting.
 * @param img start image entry
 * @return next nearest image instance or NULL next instance not found
 */
struct image* imglist_next_file(struct image* img);

/**
 * Get previous image entry given the loop setting.
 * @param img start image entry
 * @return previous nearest image instance or NULL previous instance not found
 */
struct image* imglist_prev_file(struct image* img);

/**
 * Get next image entry with different parent (another dir).
 * @param img start image entry
 * @return next nearest image instance or NULL next instance not found
 */
struct image* imglist_next_dir(struct image* img);

/**
 * Get previous image entry with different parent (another dir).
 * @param img start image entry
 * @return next previous image instance or NULL next instance not found
 */
struct image* imglist_prev_dir(struct image* img);

/**
 * Get random image entry.
 * @param img image entry to exclude (current one)
 * @return random image entry
 */
struct image* imglist_rand(struct image* img);

/**
 * Get image entry in specified distance from the start.
 * @param img start position
 * @param distance number entries to skip
 * @return image entry
 */
struct image* imglist_jump(struct image* img, ssize_t distance);

/**
 * Get distance between two image entries.
 * @param start,end image entries
 * @return number of entries in range [start,end]
 */
ssize_t imglist_distance(const struct image* start, const struct image* end);
