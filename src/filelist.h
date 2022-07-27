// SPDX-License-Identifier: MIT
// List of files to load images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stdbool.h>
#include <stddef.h>

/** File list context. */
struct file_list;

/** Types of movement through file list. */
enum file_list_move {
    fl_first_file,
    fl_last_file,
    fl_next_file,
    fl_prev_file,
    fl_next_dir,
    fl_prev_dir
};

/**
 * Initialize the file list.
 * @param files list of input source files
 * @param num number of files in the source list
 * @param recursive flag to handle directory recursively
 * @return created file list context
 */
struct file_list* flist_init(const char** files, size_t num, bool recursive);

/**
 * Free the file list.
 * @param ctx file list context
 */
void flist_free(struct file_list* ctx);

/**
 * Sort the file list alphabetically.
 * @param ctx file list context
 */
void flist_sort(struct file_list* ctx);

/**
 * Shuffle the file list.
 * @param ctx file list context
 */
void flist_shuffle(struct file_list* ctx);

/**
 * Get description of the current file in the file list.
 * @param ctx file list context
 * @param index index of the current file (started from 1)
 * @param total total number of files in the list
 * @return path to the current file or NULL if list is empty
 */
const char* flist_current(const struct file_list* ctx, size_t* index,
                          size_t* total);

/**
 * Move iterator to the specified direction.
 * @param ctx file list context
 * @param mv iterator move direction
 * @return false if no more files in the list
 */
bool flist_select(struct file_list* ctx, enum file_list_move mv);

/**
 * Exclude current file from the list and move to the next one.
 * @param ctx file list context
 * @param forward step direction for setting next file
 * @return false if no more files in the list
 */
bool flist_exclude(struct file_list* ctx, bool forward);
