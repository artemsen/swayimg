// SPDX-License-Identifier: MIT
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "filelist.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/** Number of entries added on reallocation. */
#define ALLOCATE_SIZE 32

/** File list context. */
struct file_list {
    char** paths; ///< Array of files paths
    size_t alloc; ///< Number of allocated entries (size of array)
    size_t size;  ///< Number of files in the list
    size_t index; ///< Index of the current file entry
};

/**
 * Add file to the list.
 * @param[in] list file list instance
 * @param[in] file path to the file
 */
static void add_file(file_list_t* list, const char* file)
{
    char* path;
    const size_t path_len = strlen(file) + 1 /* last null */;

    // during the initialization, the index points to the next free entry
    if (list->index >= list->alloc) {
        // relocate array
        const size_t num_entries = list->alloc + ALLOCATE_SIZE;
        char** ptr = realloc(list->paths, num_entries * sizeof(char*));
        if (!ptr) {
            return;
        }
        list->alloc = num_entries;
        list->paths = ptr;
    }

    // add new entry
    path = malloc(path_len);
    if (path) {
        memcpy(path, file, path_len);
        list->paths[list->index] = path;
        ++list->index;
        ++list->size;
    }
}

/**
 * Add files from the directory to the list.
 * @param[in] list file list instance
 * @param[in] dir full path to the directory
 * @param[in] recursive flag to handle directory recursively
 */
static void add_dir(file_list_t* list, const char* dir, bool recursive)
{
    DIR* dir_handle;
    struct dirent* dir_entry;
    struct stat file_stat;
    size_t len;
    char* path;

    dir_handle = opendir(dir);
    if (!dir_handle) {
        return;
    }

    while (true) {
        dir_entry = readdir(dir_handle);
        if (!dir_entry) {
            break;
        }
        // skip link to self/parent dirs
        if (strcmp(dir_entry->d_name, ".") == 0 ||
            strcmp(dir_entry->d_name, "..") == 0) {
            continue;
        }
        // compose full path
        len = strlen(dir) + 1 /*slash*/;
        len += strlen(dir_entry->d_name) + 1 /*last null*/;
        path = malloc(len);
        if (path) {
            strcpy(path, dir);
            strcat(path, "/");
            strcat(path, dir_entry->d_name);

            if (stat(path, &file_stat) == 0) {
                if (S_ISDIR(file_stat.st_mode)) {
                    if (recursive) {
                        add_dir(list, path, recursive);
                    }
                } else if (file_stat.st_size) {
                    add_file(list, path);
                }
            }
            free(path);
        }
    }

    closedir(dir_handle);
}

file_list_t* init_file_list(const char** files, size_t num, bool recursive)
{
    file_list_t* list;

    list = calloc(1, sizeof(*list));
    if (!list) {
        return NULL;
    }

    for (size_t i = 0; i < num; ++i) {
        struct stat file_stat;
        if (stat(files[i], &file_stat) != -1) {
            if (S_ISDIR(file_stat.st_mode)) {
                add_dir(list, files[i], recursive);
            } else {
                add_file(list, files[i]);
            }
        }
    }

    if (list->size != 0) {
        // rewind to the first entry
        list->index = 0;
    } else {
        // empty list
        free_file_list(list);
        list = NULL;
    }

    return list;
}

void free_file_list(file_list_t* list)
{
    if (list) {
        for (size_t i = 0; i < list->size; ++i) {
            free(list->paths[i]);
        }
        free(list->paths);
        free(list);
    }
}

void sort_file_list(file_list_t* list)
{
    // sort alphabetically
    for (size_t i = 0; i < list->size; ++i) {
        for (size_t j = i + 1; j < list->size; ++j) {
            if (strcoll(list->paths[i], list->paths[j]) > 0) {
                char* swap = list->paths[i];
                list->paths[i] = list->paths[j];
                list->paths[j] = swap;
            }
        }
    }
}

void shuffle_file_list(file_list_t* list)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    srand(ts.tv_nsec);

    // swap random entries
    for (size_t i = 0; i < list->size; ++i) {
        const size_t j = rand() % list->size;
        if (i != j) {
            char* swap = list->paths[i];
            list->paths[i] = list->paths[j];
            list->paths[j] = swap;
        }
    }
}

const char* get_current(const file_list_t* list, size_t* index, size_t* total)
{
    const char* current = list->size ? list->paths[list->index] : NULL;
    if (total) {
        *total = list->size;
    }
    if (index) {
        *index = list->index + 1;
    }
    return current && *current ? current : NULL;
}

bool exclude_current(file_list_t* list, bool forward)
{
    list->paths[list->index][0] = 0; // mark entry as excluded
    return next_file(list, forward);
}

bool next_file(file_list_t* list, bool forward)
{
    size_t index = list->index;

    do {
        if (forward) {
            if (++index >= list->size) {
                index = 0;
            }
        } else {
            if (index-- == 0) {
                index = list->size - 1;
            }
        }
        if (*list->paths[index]) {
            list->index = index;
            return true;
        }
    } while (index != list->index);

    // loop complete, no one valid entry found
    return false;
}

bool next_directory(file_list_t* list, bool forward)
{
    const size_t cur_index = list->index;
    const char* cur_path = list->paths[list->index];
    size_t cur_dir_len, next_dir_len;

    // directory part of the current file path
    cur_dir_len = strlen(cur_path) - 1;
    while (cur_dir_len && cur_path[cur_dir_len] != '/') {
        --cur_dir_len;
    }

    // search for another directory in file list
    while (next_file(list, forward) && list->index != cur_index) {
        // directory part of the next file path
        const char* next_path = list->paths[list->index];
        next_dir_len = strlen(next_path) - 1;
        while (next_dir_len && next_path[next_dir_len] != '/') {
            --next_dir_len;
        }
        if (cur_dir_len != next_dir_len ||
            strncmp(cur_path, next_path, cur_dir_len)) {
            return true;
        }
    }

    return false;
}
