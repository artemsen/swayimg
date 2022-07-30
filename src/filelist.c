// SPDX-License-Identifier: MIT
// List of files to load images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "filelist.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
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
    char** paths;  ///< Array of files paths
    size_t alloc;  ///< Number of allocated entries (size of array)
    size_t size;   ///< Number of files in the list
    size_t index;  ///< Index of the current file entry
    bool critical; ///< Current entry is critical, it cannot be excluded
};

/**
 * Add file to the list.
 * @param ctx file list context
 * @param file path to the file
 */
static void add_file(struct file_list* ctx, const char* file)
{
    char* path;
    size_t path_len;
    size_t file_len = strlen(file);

    // remove "./" from the start
    if (file[0] == '.' && file[1] == '/') {
        file += 2;
        file_len -= 2;
    }

    // search for duplicates
    for (size_t i = 0; i < ctx->size; ++i) {
        if (strcmp(ctx->paths[i], file) == 0) {
            return;
        }
    }

    // relocate array, if needed
    if (ctx->index >= ctx->alloc) {
        const size_t num_entries = ctx->alloc + ALLOCATE_SIZE;
        char** ptr = realloc(ctx->paths, num_entries * sizeof(char*));
        if (!ptr) {
            return;
        }
        ctx->alloc = num_entries;
        ctx->paths = ptr;
    }

    // add new entry
    path_len = file_len + 1;
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

/**
 * Step to the next file.
 * @param ctx file list context
 * @param forward step direction
 * @return false if no more files in the list
 */
static bool next_file(struct file_list* ctx, bool forward)
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
        if (ctx->paths[index]) {
            ctx->index = index;
            return true;
        }
    } while (index != ctx->index);

    // loop complete, no one valid entry found
    return false;
}

/**
 * Step to the next directory.
 * @param ctx file list context
 * @param forward step direction
 * @return false if no more files in the list
 */
static bool next_directory(struct file_list* ctx, bool forward)
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
    while (next_file(ctx, forward) && ctx->index != cur_index) {
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

/**
 * Go to the first/last file.
 * @param ctx file list context
 * @param first direction, true=first, false=last
 * @return false if no more files in the list
 */
static bool goto_file(struct file_list* ctx, bool first)
{
    ctx->index = first ? 0 : ctx->size - 1;
    if (ctx->paths[ctx->index]) {
        return true;
    }
    return next_file(ctx, first);
}

/**
 * Sort the file list alphabetically.
 * @param ctx file list context
 */
static void sort_list(struct file_list* ctx)
{
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

/**
 * Shuffle the file list.
 * @param ctx file list context
 */
static void shuffle_list(struct file_list* ctx)
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

struct file_list* flist_init(const char** files, size_t num,
                             const struct config* cfg)
{
    struct file_list* ctx;
    const char* force_start = NULL;

    ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }

    if (num == 0) {
        // no input files specified, use all from the current directory
        add_dir(ctx, ".", cfg->recursive);
    }

    for (size_t i = 0; i < num; ++i) {
        struct stat file_stat;
        if (stat(files[i], &file_stat) == -1) {
            fprintf(stderr, "%s: [%i] %s\n", files[i], errno, strerror(errno));
        } else {
            if (S_ISDIR(file_stat.st_mode)) {
                add_dir(ctx, files[i], cfg->recursive);
            } else {
                if (!cfg->all_files) {
                    add_file(ctx, files[i]);
                } else {
                    // add all files from the same directory
                    const char* delim = strrchr(files[i], '/');
                    const size_t len = delim ? delim - files[i] : 0;
                    if (len == 0) {
                        add_dir(ctx, ".", cfg->recursive);
                    } else {
                        char* dir = malloc(len + 1);
                        if (dir) {
                            memcpy(dir, files[i], len);
                            dir[len] = 0;
                            add_dir(ctx, dir, cfg->recursive);
                            free(dir);
                        }
                    }
                    if (!force_start) {
                        force_start = files[i];
                    }
                    if (num == 1) {
                        ctx->critical = true;
                    }
                }
            }
        }
    }

    if (ctx->size == 0) {
        // empty list
        flist_free(ctx);
        return NULL;
    }

    if (cfg->order == cfgord_alpha) {
        sort_list(ctx);
    } else if (cfg->order == cfgord_random) {
        shuffle_list(ctx);
    }

    // set initial index
    if (!force_start) {
        ctx->index = 0;
    } else {
        if (force_start[0] == '.' && force_start[1] == '/') {
            force_start += 2;
        }
        for (size_t i = 0; i < ctx->size; ++i) {
            if (strcmp(ctx->paths[i], force_start) == 0) {
                ctx->index = i;
                break;
            }
        }
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

const char* flist_current(const struct file_list* ctx, size_t* index,
                          size_t* total)
{
    if (total) {
        *total = ctx->size;
    }
    if (index) {
        *index = ctx->index + 1;
    }
    return ctx->size ? ctx->paths[ctx->index] : NULL;
}

bool flist_exclude(struct file_list* ctx, bool forward)
{
    if (ctx->critical) {
        return false;
    }
    free(ctx->paths[ctx->index]);
    ctx->paths[ctx->index] = NULL;
    return next_file(ctx, forward);
}

bool flist_jump(struct file_list* ctx, enum file_list_move mv)
{
    if (mv != fl_initial) {
        ctx->critical = false; // allow exclude
    }
    switch (mv) {
        case fl_initial:
            return true;
        case fl_first_file:
            return goto_file(ctx, true);
        case fl_last_file:
            return goto_file(ctx, false);
        case fl_next_file:
            return next_file(ctx, true);
        case fl_prev_file:
            return next_file(ctx, false);
        case fl_next_dir:
            return next_directory(ctx, true);
        case fl_prev_dir:
            return next_directory(ctx, false);
    }
    return false;
}
