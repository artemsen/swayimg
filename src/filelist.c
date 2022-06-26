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

/** File list entry. */
struct entry_t;
struct entry_t {
    struct entry_t* prev; ///< Pointer to the previous entry
    struct entry_t* next; ///< Pointer to the next entry
    char path[1];         ///< Full path to the file (any size array)
} __attribute__((packed));

/** File list context. */
struct file_list {
    struct entry_t* current; ///< Pointer to the current entry
    size_t size;             ///< Total number of files in the list
    size_t index;            ///< Index of current entry
};

/**
 * Add file to the list.
 * @param[in] list file list instance
 * @param[in] file path to the file
 */
static void add_file(file_list_t* list, const char* file)
{
    struct entry_t* entry;
    size_t entry_sz;

    // create new list entry
    entry_sz = sizeof(entry->prev) + sizeof(entry->next);
    entry_sz += strlen(file) + 1 /* last null */;
    entry = calloc(1, entry_sz);
    if (entry) {
        strcat(entry->path, file);

        // add entry to list
        if (list->current) {
            entry->next = list->current->next;
            entry->prev = list->current;
            list->current->next->prev = entry;
            list->current->next = entry;
            list->current = entry;
        } else {
            // first entry
            list->current = entry;
            list->current->next = entry;
            list->current->prev = entry;
        }
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
        list->current = list->current->next;
    } else {
        // empty list
        free(list);
        list = NULL;
    }

    return list;
}

void free_file_list(file_list_t* list)
{
    if (list) {
        while (list->size--) {
            struct entry_t* next = list->current->next;
            free(list->current);
            list->current = next;
        }
        free(list);
    }
}

void sort_file_list(file_list_t* list)
{
    struct entry_t** array;
    size_t index = 0;

    if (list->size <= 1) {
        return;
    }

    // create array from list
    array = malloc(sizeof(struct entry_t*) * list->size);
    if (!array) {
        return;
    }
    for (size_t i = 0; i < list->size; ++i) {
        array[index++] = list->current;
        list->current = list->current->next;
    }

    // sort alphabetically
    for (size_t i = 0; i < list->size; ++i) {
        for (size_t j = i + 1; j < list->size; ++j) {
            if (strcoll(array[i]->path, array[j]->path) > 0) {
                struct entry_t* tmp = array[i];
                array[i] = array[j];
                array[j] = tmp;
            }
        }
    }

    // relink entries
    for (size_t i = 0; i < list->size; ++i) {
        array[i]->prev = array[i ? i - 1 : list->size - 1];
        array[i]->next = array[i < list->size - 1 ? i + 1 : 0];
    }

    // reset list state
    list->current = array[0];
    list->index = 0;

    free(array);
}

void shuffle_file_list(file_list_t* list)
{
    struct entry_t** array;
    struct timespec ts;
    size_t index = 0;

    if (list->size <= 1) {
        return;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts);
    srand(ts.tv_nsec);

    // create array from list
    array = malloc(sizeof(struct entry_t*) * list->size);
    if (!array) {
        return;
    }
    for (size_t i = 0; i < list->size; ++i) {
        array[index++] = list->current;
        list->current = list->current->next;
    }

    // swap rendom entries
    for (size_t i = 0; i < list->size; ++i) {
        index = rand() % list->size;
        if (index != i) {
            struct entry_t* tmp = array[i];
            array[i] = array[index];
            array[index] = tmp;
        }
    }

    // relink entries
    for (size_t i = 0; i < list->size; ++i) {
        array[i]->prev = array[i ? i - 1 : list->size - 1];
        array[i]->next = array[i < list->size - 1 ? i + 1 : 0];
    }

    // reset list state
    list->current = array[0];
    list->index = 0;

    free(array);
}

const char* get_current(const file_list_t* list, size_t* index, size_t* total)
{
    if (total) {
        *total = list->size;
    }
    if (index) {
        *index = list->current ? list->index + 1 : 0;
    }
    return list->current ? list->current->path : NULL;
}

bool exclude_current(file_list_t* list)
{
    if (list->size == 0) {
        return false;
    } else if (list->size == 1) {
        // last entry
        free(list->current);
        list->current = NULL;
        list->size = 0;
        list->index = 0;
        return false;
    } else {
        // remove current entry
        struct entry_t* entry = list->current;
        entry->prev->next = entry->next;
        entry->next->prev = entry->prev;
        list->current = entry->next;
        free(entry);
        --list->size;
        if (list->index >= list->size) {
            list->index = 0;
        }
        return true;
    }
}

bool next_file(file_list_t* list, bool forward)
{
    if (list->size <= 1) {
        return false;
    }

    if (forward) {
        list->current = list->current->next;
        if (++list->index >= list->size) {
            list->index = 0;
        }
    } else {
        list->current = list->current->prev;
        if (list->index-- == 0) {
            list->index = list->size - 1;
        }
    }

    return true;
}

bool next_directory(file_list_t* list, bool forward)
{
    struct entry_t* current = list->current;
    size_t index = list->index;
    size_t cur_dir, chk_dir;

    if (list->size <= 1) {
        return false;
    }

    // directory part of the current entry
    cur_dir = strlen(list->current->path) - 1;
    while (cur_dir && list->current->path[cur_dir] != '/') {
        --cur_dir;
    }

    // searach for another directory in file list
    while (true) {
        if (forward) {
            current = current->next;
            if (++index >= list->size) {
                index = 0;
            }
        } else {
            current = current->prev;
            if (index-- == 0) {
                index = list->size - 1;
            }
        }
        if (current == list->current) {
            return false; // not found
        }
        // directory part of the next entry
        chk_dir = strlen(current->path) - 1;
        while (chk_dir && current->path[chk_dir] != '/') {
            --chk_dir;
        }
        if (cur_dir != chk_dir ||
            strncmp(current->path, list->current->path, cur_dir)) {
            break;
        }
    }

    // move cursor
    list->current = current;
    list->index = index;

    return true;
}
