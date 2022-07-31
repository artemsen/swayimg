// SPDX-License-Identifier: MIT
// List of files to load images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "config.h"

#include <stdbool.h>
#include <stddef.h>

/** File list context. */
struct file_list;

/** Single file entry. */
struct flist_entry {
    size_t index; ///< Entry index
    bool mark;    ///< Mark state
    char path[1]; ///< Path to the file, any size array
};

/** Types of movement direction through the file list. */
enum flist_position {
    fl_initial,
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
 * @param cfg configuration instance
 * @return created file list context
 */
struct file_list* flist_init(const char** files, size_t num,
                             const struct config* cfg);

/**
 * Free the file list.
 * @param ctx file list context
 */
void flist_free(struct file_list* ctx);

/**
 * Get file list size.
 * @return tital number of entries, includes excluded
 */
size_t flist_size(const struct file_list* ctx);

/**
 * Get description of the current file in the file list.
 * @param ctx file list context
 * @return pointer to the current entry or NULL if list is empty
 */
const struct flist_entry* flist_current(const struct file_list* ctx);

/**
 * Move iterator to the specified position.
 * @param ctx file list context
 * @param pos position to set
 * @return false if no more files in the list
 */
bool flist_jump(struct file_list* ctx, enum flist_position pos);

/**
 * Exclude current file from the list and move to the next one.
 * @param ctx file list context
 * @param forward step direction for setting next file
 * @return false if no more files in the list
 */
bool flist_exclude(struct file_list* ctx, bool forward);

/**
 * Invert mark state for the current entry.
 * @param ctx file list context
 */
void flist_mark_invcur(struct file_list* ctx);

/**
 * Invert mark state for all entries.
 * @param ctx file list context
 */
void flist_mark_invall(struct file_list* ctx);

/**
 * Mark/unmark all entries.
 * @param ctx file list context
 * @param mark state to set
 */
void flist_mark_setall(struct file_list* ctx, bool mark);

/**
 * Print the path of each marked entry.
 * @param ctx file list context
 */
void flist_mark_print(const struct file_list* ctx);
