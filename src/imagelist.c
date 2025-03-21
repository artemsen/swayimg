// SPDX-License-Identifier: MIT
// List of images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "imagelist.h"

#include "array.h"
#include "image.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/** Image list array entry. */
struct image_src {
    char* source; ///< Entry name
    time_t time;
    size_t size;
};

/** Context of the image list (which is actually an array). */
struct image_list {
    struct image_src* sources; ///< Array of entries
    size_t capacity;           ///< Number of allocated entries (size of array)
    size_t size;               ///< Number of entries in array
    enum list_order order;     ///< File list order
    bool reverse;              ///< Reverse order flag
    bool loop;                 ///< File list loop mode
    bool recursive;            ///< Read directories recursively
    bool all_files;            ///< Open all files from the same directory
};

/** Global image list instance. */
static struct image_list ctx;

/** Order names. */
static const char* order_names[] = {
    [order_none] = "none", [order_alpha] = "alpha",   [order_mtime] = "mtime",
    [order_size] = "size", [order_random] = "random",
};

/**
 * Get absolute path from relative source.
 * @param source relative file path
 * @param path output absolute path buffer
 * @param path_max size of the buffer
 * @return length of the absolute path including last null
 */
static size_t absolute_path(const char* source, char* path, size_t path_max)
{
    assert(source && path && path_max);

    char buffer[PATH_MAX];
    struct str_slice dirs[1024];
    size_t dirs_num;
    size_t pos;

    if (strcmp(source, LDRSRC_STDIN) == 0 ||
        strncmp(source, LDRSRC_EXEC, LDRSRC_EXEC_LEN) == 0) {
        strncpy(path, source, path_max - 1);
        return strlen(path) + 1;
    }

    if (*source == '/') {
        strncpy(buffer, source, sizeof(buffer) - 1);
    } else {
        // relative to the current dir
        size_t len;
        if (!getcwd(buffer, sizeof(buffer) - 1)) {
            return 0;
        }
        len = strlen(buffer);
        if (buffer[len] != '/') {
            buffer[len] = '/';
            ++len;
        }
        if (len >= sizeof(buffer)) {
            return 0;
        }
        strncpy(buffer + len, source, sizeof(buffer) - len - 1);
    }

    // split by component
    dirs_num = str_split(buffer, '/', dirs, ARRAY_SIZE(dirs));

    // remove "/../" and "/./"
    for (size_t i = 0; i < dirs_num; ++i) {
        if (dirs[i].len == 1 && dirs[i].value[0] == '.') {
            dirs[i].len = 0;
        } else if (dirs[i].len == 2 && dirs[i].value[0] == '.' &&
                   dirs[i].value[1] == '.') {
            dirs[i].len = 0;
            for (ssize_t j = (ssize_t)i - 1; j >= 0; --j) {
                if (dirs[j].len != 0) {
                    dirs[j].len = 0;
                    break;
                }
            }
        }
    }

    // collect to the absolute path
    path[0] = '/';
    pos = 1;
    for (size_t i = 0; i < dirs_num; ++i) {
        if (dirs[i].len) {
            if (pos + dirs[i].len + 1 >= path_max) {
                return 0;
            }
            memcpy(path + pos, dirs[i].value, dirs[i].len);
            pos += dirs[i].len;
            if (i < dirs_num - 1) {
                if (pos + 1 >= path_max) {
                    return 0;
                }
                path[pos++] = '/';
            }
        }
    }

    // last null
    if (pos + 1 >= path_max) {
        return 0;
    }
    path[pos++] = 0;

    return pos;
}

/**
 * Add new entry to the list.
 * @param source image data source to add
 */
static void add_entry(const char* source, struct stat* st)
{
    // check for duplicates
    for (size_t i = 0; i < ctx.size; ++i) {
        if (strcmp(ctx.sources[i].source, source) == 0) {
            return;
        }
    }

    // relocate array, if needed
    if (ctx.size + 1 >= ctx.capacity) {
        const size_t cap = ctx.capacity ? ctx.capacity * 2 : 4;
        struct image_src* ptr =
            realloc(ctx.sources, cap * sizeof(struct image_src));
        if (!ptr) {
            return;
        }
        ctx.capacity = cap;
        ctx.sources = ptr;
    }

    // add new entry
    ctx.sources[ctx.size].source = str_dup(source, NULL);
    if (ctx.sources[ctx.size].source) {
        switch (ctx.order) {
            case order_mtime:
                ctx.sources[ctx.size].time = st->st_mtime;
                break;
            case order_size:
                ctx.sources[ctx.size].size = st->st_size;
                break;
            // avoid compiler warning
            case order_alpha:
            case order_random:
            case order_none:
                break;
        }
        ++ctx.size;
    }
}

/**
 * Add file to the list.
 * @param file path to the file
 */
static void add_file(const char* path, struct stat* st)
{
    char abspath[PATH_MAX];
    if (absolute_path(path, abspath, sizeof(abspath))) {
        add_entry(abspath, st);
    }
}

/**
 * Add files from the directory to the list.
 * @param dir full path to the directory
 */
static void add_dir(const char* dir)
{
    DIR* dir_handle;
    struct dirent* dir_entry;
    char* path = NULL;

    dir_handle = opendir(dir);
    if (!dir_handle) {
        return;
    }

    while ((dir_entry = readdir(dir_handle))) {
        const char* name = dir_entry->d_name;
        struct stat st;

        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue; // skip link to self/parent dirs
        }

        // compose full path
        if (!str_dup(dir, &path) || !str_append("/", 1, &path) ||
            !str_append(name, 0, &path)) {
            continue;
        }

        if (stat(path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                if (ctx.recursive) {
                    add_dir(path);
                }
            } else if (S_ISREG(st.st_mode)) {
                add_file(path, &st);
            }
        }
    }

    free(path);
    closedir(dir_handle);
}

/**
 * Get next directory entry index (works only for paths as source).
 * @param start index of the start position
 * @param forward step direction
 * @return index of the next entry or IMGLIST_INVALID if not found
 */
static size_t next_dir(size_t start, bool forward)
{
    const char* cur_path = ctx.sources[start].source;
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

        index = image_list_nearest(index, forward, ctx.loop);
        if (index == IMGLIST_INVALID || index == start) {
            break; // not found
        }

        next_path = ctx.sources[index].source;
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
static int compare_alpha(const void* a, const void* b)
{
    int cmp =
        strcoll(((struct image_src*)a)->source, ((struct image_src*)b)->source);

    return ctx.reverse ? -cmp : cmp;
}

/**
 * Compare sources callback for `qsort`.
 * @return negative if a < b, positive if a > b, 0 otherwise
 */
static int compare_time(const void* a, const void* b)
{
    time_t ta = ((struct image_src*)a)->time;
    time_t tb = ((struct image_src*)b)->time;

    if (ta < tb) {
        return ctx.reverse ? 1 : -1;
    }
    if (ta > tb) {
        return ctx.reverse ? -1 : 1;
    }
    return 0;
}

/**
 * Compare sources callback for `qsort`.
 * @return negative if a < b, positive if a > b, 0 otherwise
 */
static int compare_size(const void* a, const void* b)
{
    size_t sa = ((struct image_src*)a)->size;
    size_t sb = ((struct image_src*)b)->size;

    if (sa < sb) {
        return ctx.reverse ? 1 : -1;
    }
    if (sa > sb) {
        return ctx.reverse ? -1 : 1;
    }
    return 0;
}

void image_list_init(const struct config* cfg)
{
    ctx.order = config_get_oneof(cfg, CFG_LIST, CFG_LIST_ORDER, order_names,
                                 ARRAY_SIZE(order_names));
    ctx.reverse = config_get_bool(cfg, CFG_LIST, CFG_LIST_REVERSE);
    ctx.loop = config_get_bool(cfg, CFG_LIST, CFG_LIST_LOOP);
    ctx.recursive = config_get_bool(cfg, CFG_LIST, CFG_LIST_RECURSIVE);
    ctx.all_files = config_get_bool(cfg, CFG_LIST, CFG_LIST_ALL);
}

void image_list_destroy(void)
{
    for (size_t i = 0; i < ctx.size; ++i) {
        free(ctx.sources[i].source);
    }
    free(ctx.sources);
    ctx.sources = NULL;
    ctx.capacity = 0;
    ctx.size = 0;
}

void image_list_add(const char* source)
{
    struct stat st;

    memset(&st, 0, sizeof(struct stat));

    // special url
    if (strncmp(source, LDRSRC_STDIN, LDRSRC_STDIN_LEN) == 0 ||
        strncmp(source, LDRSRC_EXEC, LDRSRC_EXEC_LEN) == 0) {
        add_entry(source, &st);
        return;
    }

    // file from file system
    if (stat(source, &st) != 0) {
        fprintf(stderr, "Unable to query file %s: [%i] %s\n", source, errno,
                strerror(errno));
        return;
    }
    if (S_ISDIR(st.st_mode)) {
        add_dir(source);
    } else if (S_ISREG(st.st_mode)) {
        if (!ctx.all_files) {
            add_file(source, &st);
        } else {
            // add all files from the same directory
            const char* delim = strrchr(source, '/');
            const size_t len = delim ? delim - source : 0;
            if (len == 0) {
                add_dir(".");
            } else {
                char* dir = str_append(source, len, NULL);
                if (dir) {
                    add_dir(dir);
                    free(dir);
                }
            }
        }
    }
}

void image_list_reorder(void)
{
    assert(ctx.size);

    switch (ctx.order) {
        case order_none:
            break;
        case order_alpha:
            qsort(ctx.sources, ctx.size, sizeof(*ctx.sources), compare_alpha);
            break;
        case order_mtime:
            qsort(ctx.sources, ctx.size, sizeof(*ctx.sources), compare_time);
            break;
        case order_size:
            qsort(ctx.sources, ctx.size, sizeof(*ctx.sources), compare_size);
            break;
        case order_random:
            for (size_t i = 0; i < ctx.size; ++i) {
                if (i == 0) {
                    // init random sequence
                    struct timespec ts;
                    clock_gettime(CLOCK_MONOTONIC, &ts);
                    srand(ts.tv_nsec);
                }
                const size_t j = rand() % ctx.size;
                if (i != j) {
                    struct image_src swap = ctx.sources[i];
                    ctx.sources[i] = ctx.sources[j];
                    ctx.sources[j] = swap;
                }
            }
            break;
    }
}

size_t image_list_size(void)
{
    return ctx.size;
}

const char* image_list_get(size_t index)
{
    return index < ctx.size ? ctx.sources[index].source : NULL;
}

size_t image_list_find(const char* source)
{
    char abspath[PATH_MAX];
    if (absolute_path(source, abspath, sizeof(abspath))) {
        for (size_t i = 0; i < ctx.size; ++i) {
            if (ctx.sources[i].source &&
                strcmp(ctx.sources[i].source, abspath) == 0) {
                return i;
            }
        }
    }
    return IMGLIST_INVALID;
}

size_t image_list_nearest(size_t start, bool forward, bool loop)
{
    size_t index = start;

    if (index == IMGLIST_INVALID) {
        if (forward) {
            return image_list_first();
        }
        if (loop && !forward) {
            return image_list_last();
        }
        return IMGLIST_INVALID;
    }
    if (index >= ctx.size) {
        if (!forward) {
            return image_list_last();
        }
        if (loop && forward) {
            return image_list_first();
        }
        return IMGLIST_INVALID;
    }

    while (true) {
        if (forward) {
            if (index + 1 < ctx.size) {
                ++index;
            } else if (!loop) {
                index = IMGLIST_INVALID; // already at last entry
                break;
            } else {
                index = 0;
            }
        } else {
            if (index > 0) {
                --index;
            } else if (!loop) {
                index = IMGLIST_INVALID; // already at first entry
                break;
            } else {
                index = ctx.size - 1;
            }
        }

        if (index == start) {
            index = IMGLIST_INVALID; // only one valid entry in the list
            break;
        }

        if (ctx.sources[index].source) {
            break;
        }
    }

    return index;
}

size_t image_list_jump(size_t start, size_t distance, bool forward)
{
    size_t index = start;
    if (index == IMGLIST_INVALID || index >= ctx.size) {
        return IMGLIST_INVALID;
    }

    while (distance) {
        const size_t next = image_list_nearest(index, forward, false);
        if (next == IMGLIST_INVALID) {
            break;
        }
        index = next;
        --distance;
    }

    return index;
}

size_t image_list_distance(size_t start, size_t end)
{
    size_t distance = 0;
    size_t index;

    if (start == IMGLIST_INVALID) {
        start = image_list_first();
    }
    if (end == IMGLIST_INVALID) {
        end = image_list_last();
    }
    if (start <= end) {
        index = start;
    } else {
        index = end;
        end = start;
    }

    while (index != IMGLIST_INVALID && index != end) {
        ++distance;
        index = image_list_nearest(index, true, false);
    }

    return distance;
}

size_t image_list_next_file(size_t start)
{
    return image_list_nearest(start, true, ctx.loop);
}

size_t image_list_prev_file(size_t start)
{
    return image_list_nearest(start, false, ctx.loop);
}

size_t image_list_rand_file(size_t exclude)
{
    size_t index = image_list_nearest(rand() % ctx.size, true, true);
    if (index != IMGLIST_INVALID && index == exclude) {
        index = image_list_nearest(exclude, true, true);
    }
    return index;
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
    return ctx.sources[0].source ? 0 : image_list_nearest(0, true, false);
}

size_t image_list_last(void)
{
    const size_t index = ctx.size - 1;
    if (ctx.size == 0) {
        return IMGLIST_INVALID;
    }
    return ctx.sources[index].source ? index
                                     : image_list_nearest(index, false, false);
}

size_t image_list_skip(size_t index)
{
    size_t next;

    // remove current entry from list
    if (index < ctx.size && ctx.sources[index].source) {
        free(ctx.sources[index].source);
        ctx.sources[index].source = NULL;
    }

    // get next entry
    next = image_list_nearest(index, true, false);
    if (next == IMGLIST_INVALID) {
        next = image_list_nearest(index, false, false);
    }

    return next;
}
