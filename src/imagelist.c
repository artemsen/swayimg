// SPDX-License-Identifier: MIT
// List of images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "imagelist.h"

#include "loader.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>

// Default configuration parameters
#define CFG_ORDER_DEF     "alpha"
#define CFG_LOOP_DEF      true
#define CFG_RECURSIVE_DEF false
#define CFG_ALL_DEF       true

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

/** Global image list instance. */
static struct image_list ctx;

struct autoexec {
	const char* format;
	const char* command;
};

struct autoexecs_array {
	struct autoexec* entries;
	size_t capacity;
	size_t size;
};

struct autoexecs_array autoexecs = {NULL, 0, 0};

/** Order names. */
static const char* order_names[] = {
    [order_none] = "none",
    [order_alpha] = "alpha",
    [order_reverse] = "reverse",
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

    // check for autoexec
    for (size_t i = 0; i < autoexecs.size; i++) {
	    const char* ext = strrchr(file, '.');
	    if ( ext != NULL && strcmp(ext+1, autoexecs.entries[i].format) == 0){
		    char* execFile = (char*) malloc(sizeof(char)*(LDRSRC_EXEC_LEN+strlen(autoexecs.entries[i].command)+strlen(file)+3));
		    char* execTail = strchr((const char*) autoexecs.entries[i].command, (int) '%');
		    char* execHead = strncpy((char*)malloc(sizeof(char)*(strlen(autoexecs.entries[i].command)-strlen(execTail)+1)), 
			autoexecs.entries[i].command, strlen(autoexecs.entries[i].command)-strlen(execTail));
		    sprintf(execFile, "%s%s%s%s", LDRSRC_EXEC, execHead, file, execTail+1);
		    add_entry(execFile);
		    free(execHead);
		    return;
	    }
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

        index = image_list_nearest(index, forward, ctx.loop);
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
static int compare_alpha(const void* a, const void* b)
{
    return strcoll(*(const char**)a, *(const char**)b);
}

/**
 * Compare sources callback for `qsort`.
 * @return negative if a > b, positive if a < b, 0 otherwise
 */
static int compare_reverse(const void* a, const void* b)
{
    return -strcoll(*(const char**)a, *(const char**)b);
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
 * Load config.
 * @param cfg config instance
 */
static void load_config(struct config* cfg)
{
    ssize_t index;
    const char* order;

    // list order
    ctx.order = order_alpha;
    order =
        config_get_string(cfg, IMGLIST_SECTION, IMGLIST_ORDER, CFG_ORDER_DEF);
    index = str_index(order_names, order, 0);
    if (index >= 0) {
        ctx.order = index;
    } else {
        config_error_val(IMGLIST_SECTION, IMGLIST_ORDER);
    }

    // list modes
    ctx.loop =
        config_get_bool(cfg, IMGLIST_SECTION, IMGLIST_LOOP, CFG_LOOP_DEF);
    ctx.recursive = config_get_bool(cfg, IMGLIST_SECTION, IMGLIST_RECURSIVE,
                                    CFG_RECURSIVE_DEF);
    ctx.all_files =
        config_get_bool(cfg, IMGLIST_SECTION, IMGLIST_ALL, CFG_ALL_DEF);

    // autoexec

    const char* autoexec_formats = config_get_string(cfg, IMGLIST_AUTOEXEC, AUTOEXEC_FORMATS, NULL);
    if ( autoexec_formats == NULL ) return;
    char* autoexec_iterator = malloc(sizeof(char)*(strlen(autoexec_formats)+1));
    strcpy(autoexec_iterator, autoexec_formats);
    char* autoexec_format_entry = strtok(autoexec_iterator, ",");
    autoexecs.entries = (struct autoexec*) malloc(sizeof(struct autoexec));
    autoexecs.capacity = 1;
    while ( autoexec_format_entry != NULL ) {

	    struct autoexec entry;
	    entry.format = autoexec_format_entry;

	    entry.command = config_get_string(cfg, IMGLIST_AUTOEXEC, entry.format, NULL);

	    if ( entry.command == NULL ) fprintf(stderr ,"WARNING: No corresponding command for %s autoexec format", entry.format);

	    else {
		if (autoexecs.size >= autoexecs.capacity){
			struct autoexec* entries_new = (struct autoexec*) malloc(sizeof(struct autoexec)*autoexecs.capacity*2);
			memcpy(entries_new, autoexecs.entries, sizeof(struct autoexec)*autoexecs.size);
			free(autoexecs.entries);
			autoexecs.entries = entries_new;
			autoexecs.capacity *= 2;
		}

		autoexecs.entries[autoexecs.size] = entry;
		autoexecs.size++; 
	    }
	    autoexec_format_entry = strtok(NULL, ",");
    }
}

size_t image_list_init(struct config* cfg, const char** sources, size_t num)
{
    struct stat file_stat;

    load_config(cfg);

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
        // sort list
        switch (ctx.order) {
            case order_none:
                break;
            case order_alpha:
                qsort(ctx.sources, ctx.size, sizeof(*ctx.sources),
                      compare_alpha);
                break;
            case order_reverse:
                qsort(ctx.sources, ctx.size, sizeof(*ctx.sources),
                      compare_reverse);
                break;
            case order_random:
                shuffle_list();
                break;
        }
    }

    return ctx.size;
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

        if (ctx.sources[index]) {
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
    return ctx.sources[0] ? 0 : image_list_nearest(0, true, false);
}

size_t image_list_last(void)
{
    const size_t index = ctx.size - 1;
    if (ctx.size == 0) {
        return IMGLIST_INVALID;
    }
    return ctx.sources[index] ? index : image_list_nearest(index, false, false);
}

size_t image_list_skip(size_t index)
{
    size_t next;

    // remove current entry from list
    if (index < ctx.size && ctx.sources[index]) {
        free(ctx.sources[index]);
        ctx.sources[index] = NULL;
    }

    // get next entry
    next = image_list_nearest(index, true, false);
    if (next == IMGLIST_INVALID) {
        next = image_list_nearest(index, false, false);
    }

    return next;
}
