// SPDX-License-Identifier: MIT
// Image loader.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.h"

/**
 * Initialize background thread loader.
 */
void loader_init(void);

/**
 * Destroy background thread loader.
 */
void loader_destroy(void);

/**
 * Load image from specified source.
 * @param source image data source: path to the file, exec command, etc
 * @param image pointer to output image instance
 * @return loading status
 */
enum loader_status loader_from_source(const char* source, struct image** image);

/**
 * Load image with specified index in the image list.
 * @param index index of the entry in the image list
 * @param image pointer to output image instance
 * @return loading status
 */
enum loader_status loader_from_index(size_t index, struct image** image);

/**
 * Append image to background loader queue.
 * @param index index of the image in the image list
 */
void loader_queue_append(size_t index);

/**
 * Reset background loader queue.
 */
void loader_queue_reset(void);
