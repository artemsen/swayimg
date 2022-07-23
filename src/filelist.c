// SPDX-License-Identifier: MIT
// List of files to load images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "filelist.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
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
 * @param ctx file list context
 * @param file path to the file
 */
static void add_file(struct file_list* ctx, const char* file)
{
    char* path;
    const size_t path_len = strlen(file) + 1 /* last null */;

    // during the initialization, the index points to the next free entry
    if (ctx->index >= ctx->alloc) {
        // relocate array
        const size_t num_entries = ctx->alloc + ALLOCATE_SIZE;
        char** ptr = realloc(ctx->paths, num_entries * sizeof(char*));
        if (!ptr) {
            return;
        }
        ctx->alloc = num_entries;
        ctx->paths = ptr;
    }

    // add new entry
    path = malloc(path_len);
    if (path) {
        memcpy(path, file, path_len);
        ctx->paths[ctx->index] = path;
        ++ctx->index;
        ++ctx->size;
    }
}

/**
 * Add files from the directory to the list.
 * @param ctx file list context
 * @param dir full path to the directory
 * @param recursive flag to handle directory recursively
 */
static void add_dir(struct file_list* ctx, const char* dir, bool recursive)
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
                        add_dir(ctx, path, recursive);
                    }
                } else if (file_stat.st_size) {
                    add_file(ctx, path);
                }
            }
            free(path);
        }
    }

    closedir(dir_handle);
}

struct file_list* flist_init(const char** files, size_t num, bool recursive)
{
    struct file_list* ctx;

    ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }

    for (size_t i = 0; i < num; ++i) {
        struct stat file_stat;
        if (stat(files[i], &file_stat) == -1) {
            fprintf(stderr, "Unable to open %s: [%i] %s\n", files[i], errno,
                    strerror(errno));
        } else {
            if (S_ISDIR(file_stat.st_mode)) {
                add_dir(ctx, files[i], recursive);
            } else {
                add_file(ctx, files[i]);
            }
        }
    }

    if (ctx->size != 0) {
        // rewind to the first entry
        ctx->index = 0;
    } else {
        // empty list
        flist_free(ctx);
        ctx = NULL;
    }

    return ctx;
}

void flist_free(struct file_list* ctx)
{
    if (ctx) {
        for (size_t i = 0; i < ctx->size; ++i) {
            free(ctx->paths[i]);
        }
        free(ctx->paths);
        free(ctx);
    }
}

void flist_sort(struct file_list* ctx)
{
    // sort alphabetically
    for (size_t i = 0; i < ctx->size; ++i) {
        for (size_t j = i + 1; j < ctx->size; ++j) {
            if (strcoll(ctx->paths[i], ctx->paths[j]) > 0) {
                char* swap = ctx->paths[i];
                ctx->paths[i] = ctx->paths[j];
                ctx->paths[j] = swap;
            }
        }
    }
}

void flist_shuffle(struct file_list* ctx)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    srand(ts.tv_nsec);

    // swap random entries
    for (size_t i = 0; i < ctx->size; ++i) {
        const size_t j = rand() % ctx->size;
        if (i != j) {
            char* swap = ctx->paths[i];
            ctx->paths[i] = ctx->paths[j];
            ctx->paths[j] = swap;
        }
    }
}

const char* flist_current(const struct file_list* ctx, size_t* index,
                          size_t* total)
{
    const char* current = ctx->size ? ctx->paths[ctx->index] : NULL;
    if (total) {
        *total = ctx->size;
    }
    if (index) {
        *index = ctx->index + 1;
    }
    return current && *current ? current : NULL;
}

bool flist_exclude(struct file_list* ctx, bool forward)
{
    ctx->paths[ctx->index][0] = 0; // mark entry as excluded
    return flist_next_file(ctx, forward);
}

bool flist_next_file(struct file_list* ctx, bool forward)
{
    size_t index = ctx->index;

    do {
        if (forward) {
            if (++index >= ctx->size) {
                index = 0;
            }
        } else {
            if (index-- == 0) {
                index = ctx->size - 1;
            }
        }
        if (*ctx->paths[index]) {
            ctx->index = index;
            return true;
        }
    } while (index != ctx->index);

    // loop complete, no one valid entry found
    return false;
}

bool flist_next_directory(struct file_list* ctx, bool forward)
{
    const size_t cur_index = ctx->index;
    const char* cur_path = ctx->paths[ctx->index];
    size_t cur_dir_len, next_dir_len;

    // directory part of the current file path
    cur_dir_len = strlen(cur_path) - 1;
    while (cur_dir_len && cur_path[cur_dir_len] != '/') {
        --cur_dir_len;
    }

    // search for another directory in file list
    while (flist_next_file(ctx, forward) && ctx->index != cur_index) {
        // directory part of the next file path
        const char* next_path = ctx->paths[ctx->index];
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
