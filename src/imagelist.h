// SPDX-License-Identifier: MIT
// List of images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.h"

/** Image entry. */
struct image_entry {
    size_t index;        ///< Entry index in the list
    struct image* image; ///< Image handle
};

/** Movement directions. */
enum list_jump {
    jump_first_file,
    jump_last_file,
    jump_next_file,
    jump_prev_file,
    jump_next_dir,
    jump_prev_dir
};

/**
 * Initialize the image list.
 * @param files list of input files
 * @param num number of files in the file list
 * @return false if no one images loaded
 */
bool image_list_init(const char** files, size_t num);

/**
 * Free the image list.
 */
void image_list_free(void);

/**
 * Get image list size.
 * @return tital number of entries in the list include non-image files
 */
size_t image_list_size(void);

/**
 * Get current entry in the image list.
 * @return current entry description
 */
struct image_entry image_list_current(void);

/**
 * Execute system command for the current entry.
 * @return error code from the system call
 */
int image_list_exec(void);

/**
 * Reset cache and reload current image.
 * @return false if reset failed (no more images)
 */
bool image_list_reset(void);

/**
 * Move through image list.
 * @param jump position to set
 * @return false if iterator can not be moved
 */
bool image_list_jump(enum list_jump jump);
