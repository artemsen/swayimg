// SPDX-License-Identifier: MIT
// List of images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "imagelist.h"

#include "config.h"
#include "loader.h"
#include "memdata.h"
#include "ui.h"
#include "viewer.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

/** Context of the image list (which is actually an array). */
struct image_list {
    char** sources;        ///< Array of entries
    size_t capacity;       ///< Number of allocated entries (size of array)
    size_t size;           ///< Number of entries in array
    enum list_order order; ///< File list order
    bool loop;             ///< File list loop mode
    bool recursive;        ///< Read directories recursively
    bool all_files;        ///< Open all files from the same directory
};
static struct image_list ctx = {
    .order = order_alpha,
    .loop = true,
    .recursive = false,
    .all_files = true,
};

/** Order names. */
static const char* order_names[] = {
    [order_none] = "none",
    [order_alpha] = "alpha",
    [order_random] = "random",
};

/**
 * Add new entry to the list.
 * @param source image data source to add
 */
static void add_entry(const char* source)
{
    // check for duplicates
    for (size_t i = 0; i < ctx.size; ++i) {
        if (strcmp(ctx.sources[i], source) == 0) {
            return;
        }
    }

    // relocate array, if needed
    if (ctx.size + 1 >= ctx.capacity) {
        const size_t cap = ctx.capacity ? ctx.capacity * 2 : 4;
        char** ptr = realloc(ctx.sources, cap * sizeof(*ctx.sources));
        if (!ptr) {
            return;
        }
        ctx.capacity = cap;
        ctx.sources = ptr;
    }

    // add new entry
    ctx.sources[ctx.size] = str_dup(source, NULL);
    if (ctx.sources[ctx.size]) {
        ++ctx.size;
    }
}

/**
 * Add file to the list.
 * @param file path to the file
 */
static void add_file(const char* file)
{
    // remove "./" from file path
    if (file[0] == '.' && file[1] == '/') {
        file += 2;
    }

    add_entry(file);
}

/**
 * Add files from the directory to the list.
 * @param dir full path to the directory
 * @param recursive flag to handle directory recursively
 */
static void add_dir(const char* dir, bool recursive)
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
            // NOLINTBEGIN(clang-analyzer-security.insecureAPI.strcpy)
            strcpy(path, dir);
            strcat(path, "/");
            strcat(path, dir_entry->d_name);
            // NOLINTEND(clang-analyzer-security.insecureAPI.strcpy)

            if (stat(path, &file_stat) == 0) {
                if (S_ISDIR(file_stat.st_mode)) {
                    if (recursive) {
                        add_dir(path, recursive);
                    }
                } else {
                    add_file(path);
                }
            }
            free(path);
        }
    }

    closedir(dir_handle);
}

/**
 * Get next source entry.
 * @param start index of the start position
 * @param forward step direction
 * @return index of the next entry or IMGLIST_INVALID if not found
 */
static size_t next_entry(size_t start, bool forward)
{
    size_t index = start;

    if (start == IMGLIST_INVALID) {
        return image_list_first();
    }

    while (true) {
        if (forward) {
            if (++index >= ctx.size) {
                if (!ctx.loop) {
                    break;
                }
                index = 0;
            }
        } else {
            if (index-- == 0) {
                if (!ctx.loop) {
                    break;
                }
                index = ctx.size - 1;
            }
        }

        if (index == start) {
            break;
        }

        if (ctx.sources[index]) {
            return index;
        }
    }

    return IMGLIST_INVALID;
}

/**
 * Get next directory entry index (works only for paths as source).
 * @param start index of the start position
 * @param forward step direction
 * @return index of the next entry or IMGLIST_INVALID if not found
 */
static size_t next_dir(size_t start, bool forward)
{
    const char* cur_path = ctx.sources[start];
    size_t cur_len;
    size_t index = start;

    if (start == IMGLIST_INVALID) {
        return image_list_first();
    }

    // directory part of the current file path
    cur_len = strlen(cur_path) - 1;
    while (cur_len && cur_path[cur_len] != '/') {
        --cur_len;
    }

    // search for another directory in file list
    while (true) {
        const char* next_path;
        size_t next_len;

        index = next_entry(index, forward);
        if (index == IMGLIST_INVALID || index == start) {
            break; // not found
        }

        next_path = ctx.sources[index];
        next_len = strlen(next_path) - 1;
        while (next_len && next_path[next_len] != '/') {
            --next_len;
        }
        if (cur_len != next_len || strncmp(cur_path, next_path, next_len)) {
            return index;
        }
    };

    return IMGLIST_INVALID;
}

/**
 * Compare sources callback for `qsort`.
 * @return negative if a < b, positive if a > b, 0 otherwise
 */
static int compare_sources(const void* a, const void* b)
{
    return strcoll(*(const char**)a, *(const char**)b);
}

/**
 * Shuffle the image list.
 */
static void shuffle_list(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    srand(ts.tv_nsec);

    // swap random entries
    for (size_t i = 0; i < ctx.size; ++i) {
        const size_t j = rand() % ctx.size;
        if (i != j) {
            char* swap = ctx.sources[i];
            ctx.sources[i] = ctx.sources[j];
            ctx.sources[j] = swap;
        }
    }
}

/**
 * Custom section loader, see `config_loader` for details.
 */
static enum config_status load_config(const char* key, const char* value)
{
    enum config_status status = cfgst_invalid_value;

    if (strcmp(key, IMGLIST_CFG_ORDER) == 0) {
        const ssize_t index = str_index(order_names, value, 0);
        if (index >= 0) {
            ctx.order = index;
            status = cfgst_ok;
        }
    } else if (strcmp(key, IMGLIST_CFG_LOOP) == 0) {
        if (config_to_bool(value, &ctx.loop)) {
            status = cfgst_ok;
        }
    } else if (strcmp(key, IMGLIST_CFG_RECURSIVE) == 0) {
        if (config_to_bool(value, &ctx.recursive)) {
            status = cfgst_ok;
        }
    } else if (strcmp(key, IMGLIST_CFG_ALL) == 0) {
        if (config_to_bool(value, &ctx.all_files)) {
            status = cfgst_ok;
        }
    } else {
        status = cfgst_invalid_key;
    }

    return status;
}

void image_list_create(void)
{
    // register configuration loader
    config_add_loader(IMGLIST_CFG_SECTION, load_config);
}

void image_list_destroy(void)
{
    for (size_t i = 0; i < ctx.size; ++i) {
        free(ctx.sources[i]);
    }
    free(ctx.sources);
    ctx.sources = NULL;
    ctx.capacity = 0;
    ctx.size = 0;
}

size_t image_list_init(const char** sources, size_t num)
{
    struct stat file_stat;

    for (size_t i = 0; i < num; ++i) {
        // special files
        if (strncmp(sources[i], LDRSRC_STDIN, LDRSRC_STDIN_LEN) == 0 ||
            strncmp(sources[i], LDRSRC_EXEC, LDRSRC_EXEC_LEN) == 0) {
            add_entry(sources[i]);
            continue;
        }
        // file system files
        if (stat(sources[i], &file_stat) != 0) {
            continue;
        }
        if (S_ISDIR(file_stat.st_mode)) {
            add_dir(sources[i], ctx.recursive);
            continue;
        }
        if (!ctx.all_files) {
            add_file(sources[i]);
            continue;
        }
        // add all files from the same directory
        const char* delim = strrchr(sources[i], '/');
        const size_t len = delim ? delim - sources[i] : 0;
        if (len == 0) {
            add_dir(".", ctx.recursive);
        } else {
            char* dir = malloc(len + 1);
            if (dir) {
                memcpy(dir, sources[i], len);
                dir[len] = 0;
                add_dir(dir, ctx.recursive);
                free(dir);
            }
        }
    }

    if (ctx.size != 0) {
        // sort or shuffle
        if (ctx.order == order_alpha) {
            qsort(ctx.sources, ctx.size, sizeof(*ctx.sources), compare_sources);
        } else if (ctx.order == order_random) {
            shuffle_list();
        }
    }

    return ctx.size;
}

size_t image_list_size(void)
{
    return ctx.size;
}

const char* image_list_get(size_t index)
{
    return index < ctx.size ? ctx.sources[index] : NULL;
}

size_t image_list_find(const char* source)
{
    // remove "./" from file source
    if (source[0] == '.' && source[1] == '/') {
        source += 2;
    }
    for (size_t i = 0; i < ctx.size; ++i) {
        if (ctx.sources[i] && strcmp(ctx.sources[i], source) == 0) {
            return i;
        }
    }
    return IMGLIST_INVALID;
}

size_t image_list_distance(size_t start, size_t end)
{
    size_t index = start;
    size_t step = 0;

    if (start > end) {
        return IMGLIST_INVALID;
    }

    while (index != IMGLIST_INVALID && index < end) {
        ++step;
        index = next_entry(index, true);
    }

    return step;
}

size_t image_list_back(size_t start, size_t distance)
{
    size_t index = start;
    while (distance--) {
        const size_t next = next_entry(index, false);
        if (next == IMGLIST_INVALID || next >= start) {
            break;
        }
        index = next;
    }
    return index;
}

size_t image_list_forward(size_t start, size_t distance)
{
    size_t index = start;
    while (distance--) {
        const size_t next = next_entry(index, true);
        if (next == IMGLIST_INVALID || next <= start) {
            break;
        }
        index = next;
    }
    return index;
}

size_t image_list_next_file(size_t start)
{
    return next_entry(start, true);
}

size_t image_list_prev_file(size_t start)
{
    return next_entry(start, false);
}

size_t image_list_next_dir(size_t start)
{
    return next_dir(start, true);
}

size_t image_list_prev_dir(size_t start)
{
    return next_dir(start, false);
}

size_t image_list_first(void)
{
    if (ctx.size == 0) {
        return IMGLIST_INVALID;
    }
    return ctx.sources[0] ? 0 : next_entry(0, true);
}

size_t image_list_last(void)
{
    const size_t index = ctx.size - 1;
    if (ctx.size == 0) {
        return IMGLIST_INVALID;
    }
    return ctx.sources[index] ? index : next_entry(index, false);
}

size_t image_list_skip(size_t index)
{
    if (index < ctx.size && ctx.sources[index]) {
        // remove current entry from list
        free(ctx.sources[index]);
        ctx.sources[index] = NULL;
    }
    return next_entry(index, true);
}
