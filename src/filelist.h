// SPDX-License-Identifier: MIT
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stdbool.h>
#include <stddef.h>

/** File list context. */
struct file_list;
typedef struct file_list file_list_t;

/**
 * Initialize the file list.
 * @param[in] files list of input source files
 * @param[in] num number of files in the source list
 * @param[in] recursive flag to handle directory recursively
 * @return created file list context
 */
file_list_t* init_file_list(const char** files, size_t num, bool recursive);

/**
 * Free the file list.
 * @param[in] list file list instance
 */
void free_file_list(file_list_t* list);

/**
 * Sort the file list alphabetically.
 * @param[in] list file list instance
 */
void sort_file_list(file_list_t* list);

/**
 * Shuffle the file list.
 * @param[in] list file list instance
 */
void shuffle_file_list(file_list_t* list);

/**
 * Get description of the current file in the file list.
 * @param[in] list file list instance
 * @param[out] index index of the current file (started from 1)
 * @param[out] total total number of files in the list
 * @return path to the current file or NULL if list is empty
 */
const char* get_current(const file_list_t* list, size_t* index, size_t* total);

/**
 * Exclude current file from the list and move to the next one.
 * @param[in] list file list instance
 * @param[in] forward step direction for setting next file
 * @return false if no more files in the list
 */
bool exclude_current(file_list_t* list, bool forward);

/**
 * Step to the next file.
 * @param[in] list file list instance
 * @param[in] forward step direction
 * @return false if no more files in the list
 */
bool next_file(file_list_t* list, bool forward);

/**
 * Step to the next directory.
 * @param[in] list file list instance
 * @param[in] forward step direction
 * @return false if no more files in the list
 */
bool next_directory(file_list_t* list, bool forward);
