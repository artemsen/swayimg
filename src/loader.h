// SPDX-License-Identifier: MIT
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * Initialize image loader.
 * @param[in] files file list
 * @param[in] count total number of files in list
 */
void loader_init(const char** files, size_t count);

/**
 * Free resources used by image loader.
 */
void loader_free(void);

/**
 * Load next/previous image file.
 * @param[in] forward iterator direction
 * @return image instance or NULL on errors
 */
struct image* load_next_file(bool forward);
