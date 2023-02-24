// SPDX-License-Identifier: MIT
// List of images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "config.h"
#include "image.h"

#include <stdbool.h>
#include <stddef.h>

/** Image list context. */
struct image_list;

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
 * @param cfg configuration instance
 * @return created image list context
 */
struct image_list* image_list_init(const char** files, size_t num,
                                   const struct config* cfg);

/**
 * Free the image list.
 * @param ctx image list context
 */
void image_list_free(struct image_list* ctx);

/**
 * Get image list size.
 * @return tital number of entries in the list include non-image files
 */
size_t image_list_size(const struct image_list* ctx);

/**
 * Get current entry in the image list.
 * @param ctx image list context
 * @return current entry description
 */
struct image_entry image_list_current(const struct image_list* ctx);

/**
 * Execute system command for the current entry.
 * @param ctx image list context
 * @return error code from the system call
 */
int image_list_cur_exec(const struct image_list* ctx);

/**
 * Move through image list.
 * @param ctx image list context
 * @param jump position to set
 * @return false if iterator can not be moved
 */
bool image_list_jump(struct image_list* ctx, enum list_jump jump);
