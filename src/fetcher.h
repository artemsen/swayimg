// SPDX-License-Identifier: MIT
// Images origin for viewer mode.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.h"

/**
 * Initialize global fetch context.
 * @param image initial image
 * @param history max number of images in history
 * @param preload max number of preloaded images
 */
void fetcher_init(struct image* image, size_t history, size_t preload);

/**
 * Destroy global fetch context.
 */
void fetcher_destroy(void);

/**
 * Reset cache and reload current image.
 * @param index preferable index of image in the image list
 * @param force flag to fail if loading specified image failed
 * @return true if image was reloaded
 */
bool fetcher_reset(size_t index, bool force);

/**
 * Open image and set it as the current one.
 * @param index index of the image to fetch
 * @return true if image opened
 */
bool fetcher_open(size_t index);

/**
 * Attach image to preload cache.
 * @param image loaded image instance, NULL if load error
 * @param index index of the image in the image list
 */
void fetcher_attach(struct image* image, size_t index);

/**
 * Get current image.
 * @return current image or NULL if no image loaded yet
 */
struct image* fetcher_current(void);
