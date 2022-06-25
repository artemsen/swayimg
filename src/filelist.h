// SPDX-License-Identifier: MIT
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stdbool.h>
#include <stddef.h>

/** File list context. */
struct file_list;
typedef struct file_list file_list_t;

/**
 * Initialize file list.
 * @param[in] files list of input source files
 * @param[in] num number of files in the source list
 * @param[in] recursive flag to handle directory recursively
 * @return created file list context
 */
file_list_t* init_file_list(const char** files, size_t num, bool recursive);

/**
 * Free file list.
 * @param[in] list file list instance
 */
void free_file_list(file_list_t* list);

/**
 * Sort file list alphabetically.
 * @param[in] list file list instance
 */
void sort_file_list(file_list_t* list);

/**
 * Shuffle file list.
 * @param[in] list file list instance
 */
void shuffle_file_list(file_list_t* list);

/**
 * Get description of current file in list.
 * @param[in] list file list instance
 * @param[out] index index of the current file (started from 1)
 * @param[out] total total number of files in the list
 * @return path to the current file or NULL if list is empty
 */
const char* file_list_current(const file_list_t* list, size_t* index,
                              size_t* total);

/**
 * Step to the next file in list.
 * @param[in] list file list instance
 * @return false if no more files in the list
 */
bool file_list_next(file_list_t* list);

/**
 * Step to the previous file in list.
 * @param[in] list file list instance
 * @return false if no more files in the list
 */
bool file_list_prev(file_list_t* list);

/**
 * Remove current file from the list.
 * @param[in] list file list instance
 * @return false if no more files in the list
 */
bool file_list_skip(file_list_t* list);
