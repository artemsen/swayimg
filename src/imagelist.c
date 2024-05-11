// SPDX-License-Identifier: MIT
// List of images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "imagelist.h"

#include "buildcfg.h"
#include "config.h"
#include "str.h"
#include "ui.h"
#include "viewer.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_INOTIFY
#include <sys/inotify.h>
#endif

/** Number of entries added on reallocation. */
#define ALLOCATE_SIZE 32
/** Name used for image, that is read from stdin through pipe. */
#define STDIN_FILE_NAME "*stdin*"

/** Single list entry. */
struct entry {
    size_t index; ///< Entry index in the list
    char path[1]; ///< Path to the file, any size array
};

/** Image list context. */
struct image_list {
    struct entry** entries; ///< Array of entries
    size_t alloc;           ///< Number of allocated entries (size of array)
    size_t size;            ///< Number of files in array
    size_t index;           ///< Index of the current file entry
    struct image* prev;     ///< Previous image handle (cached)
    struct image* current;  ///< Current image handle
    struct image* next;     ///< Next image handle (preload)
    pthread_t preloader;    ///< Preload thread
    enum list_order order;  ///< File list order
    bool loop;              ///< File list loop mode
    bool recursive;         ///< Read directories recursively
    bool all_files;         ///< Open all files from the same directory
#ifdef HAVE_INOTIFY
    int notify; ///< inotify file handler
    int watch;  ///< Current file watcher
#endif          // HAVE_INOTIFY
};
static struct image_list ctx = {
    .order = order_alpha,
    .loop = true,
    .recursive = false,
    .all_files = true,
#ifdef HAVE_INOTIFY
    .notify = -1,
    .watch = -1,
#endif // HAVE_INOTIFY
};

/** Order names. */
static const char* order_names[] = {
    [order_none] = "none",
    [order_alpha] = "alpha",
    [order_random] = "random",
};

/**
 * Add file to the list.
 * @param file path to the file
 */
static void add_file(const char* file)
{
    struct entry* entry;
    size_t entry_sz;
    size_t path_len = strlen(file);

    // remove "./" from the start
    if (file[0] == '.' && file[1] == '/') {
        file += 2;
        path_len -= 2;
    }

    // search for duplicates
    for (size_t i = 0; i < ctx.size; ++i) {
        if (strcmp(ctx.entries[i]->path, file) == 0) {
            return;
        }
    }

    // relocate array, if needed
    if (ctx.index >= ctx.alloc) {
        const size_t num = ctx.alloc + ALLOCATE_SIZE;
        struct entry** ptr = realloc(ctx.entries, num * sizeof(struct entry*));
        if (!ptr) {
            return;
        }
        ctx.alloc = num;
        ctx.entries = ptr;
    }

    // add new entry
    entry_sz = sizeof(struct entry) + path_len;
    entry = malloc(entry_sz);
    if (entry) {
        memcpy(entry->path, file, path_len + 1);
        entry->index = ctx.index;
        ctx.entries[ctx.index] = entry;
        ++ctx.index;
        ++ctx.size;
    }
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
            strcpy(path, dir);
            strcat(path, "/");
            strcat(path, dir_entry->d_name);

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
 * Peek next file entry.
 * @param start index of the start position
 * @param forward step direction
 * @return index of the next entry or SIZE_MAX if not found
 */
static size_t peek_next_file(size_t start, bool forward)
{
    size_t index = start;

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

        if (ctx.entries[index]) {
            return index;
        }
    }

    return SIZE_MAX;
}

/**
 * Peek next directory entry.
 * @param file path to extarct source directory
 * @param start index of the start position
 * @param forward step direction
 * @return index of the next entry or SIZE_MAX if not found
 */
static size_t peek_next_dir(const char* file, size_t start, bool forward)
{
    const char* cur_path = file;
    size_t cur_len;
    size_t index = start;

    // directory part of the current file path
    cur_len = strlen(cur_path) - 1;
    while (cur_len && cur_path[cur_len] != '/') {
        --cur_len;
    }

    // search for another directory in file list
    while (true) {
        const char* next_path;
        size_t next_len;

        index = peek_next_file(index, forward);
        if (index == SIZE_MAX || index == start) {
            break; // not found
        }

        next_path = ctx.entries[index]->path;
        next_len = strlen(next_path) - 1;
        while (next_len && next_path[next_len] != '/') {
            --next_len;
        }
        if (cur_len != next_len || strncmp(cur_path, next_path, next_len)) {
            return index;
        }
    };

    return SIZE_MAX;
}

/**
 * Peek first/last entry.
 * @param first direction, true=first, false=last
 * @return index of the next entry or SIZE_MAX if not found
 */
static size_t peek_edge(bool first)
{
    size_t index = first ? 0 : ctx.size - 1;
    if (index == ctx.index || ctx.entries[index]) {
        return index;
    }
    return peek_next_file(ctx.index, first);
}

/**
 * Compare paths with strcoll
 * @return negative if a < b, positive if a > b, 0 otherwise
 */
static int compare_paths(const void* a, const void* b)
{
    const struct entry* entry_a = *(const struct entry**)a;
    const struct entry* entry_b = *(const struct entry**)b;

    return strcoll(entry_a->path, entry_b->path);
}

/**
 * Sort the image list alphabetically.
 */
static void sort_list(void)
{
    qsort(ctx.entries, ctx.size, sizeof(struct entry*), compare_paths);
    for (size_t i = 0; i < ctx.size; ++i) {
        ctx.entries[i]->index = i;
    }
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
            struct entry* swap = ctx.entries[i];
            ctx.entries[i] = ctx.entries[j];
            ctx.entries[j] = swap;
        }
    }
}

/** Image preloader executed in background thread. */
static void* preloader_thread(__attribute__((unused)) void* data)
{
    size_t index = ctx.index;

    while (true) {
        struct image* img;

        index = peek_next_file(index, true);
        if (index == SIZE_MAX) {
            break; // next image not found
        }
        if ((ctx.next && ctx.next->file_path == ctx.entries[index]->path) ||
            (ctx.prev && ctx.prev->file_path == ctx.entries[index]->path)) {
            break; // already loaded
        }

        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

        img = image_from_file(ctx.entries[index]->path);
        if (img) {
            image_free(ctx.next);
            ctx.next = img;
            break;
        } else {
            // not an image, remove entry from list
            free(ctx.entries[index]);
            ctx.entries[index] = NULL;
        }

        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    }

    return NULL;
}

/**
 * Stop or restart background thread to preload adjacent images.
 * @param restart action: true=restart, false=stop preloader
 */
static void preloader_ctl(bool restart)
{
    if (ctx.preloader) {
        pthread_cancel(ctx.preloader);
        pthread_join(ctx.preloader, NULL);
        ctx.preloader = 0;
    }
    if (restart) {
        pthread_create(&ctx.preloader, NULL, preloader_thread, NULL);
    }
}

#ifdef HAVE_INOTIFY
/**
 * Register watcher for current file.
 */
static void watch_current(void)
{
    if (ctx.notify >= 0) {
        if (ctx.watch != -1) {
            inotify_rm_watch(ctx.notify, ctx.watch);
            ctx.watch = -1;
        }
        ctx.watch = inotify_add_watch(ctx.notify, ctx.current->file_path,
                                      IN_CLOSE_WRITE | IN_MOVE_SELF);
    }
}

/**
 * Notify handler.
 */
static void on_notify(void)
{
    while (true) {
        bool updated = false;
        uint8_t buffer[1024];
        ssize_t pos = 0;
        const ssize_t len = read(ctx.notify, buffer, sizeof(buffer));

        if (len < 0) {
            if (errno == EINTR) {
                continue;
            }
            return; // something went wrong
        }

        while (pos + sizeof(struct inotify_event) <= (size_t)len) {
            const struct inotify_event* event =
                (struct inotify_event*)&buffer[pos];
            if (event->mask & IN_IGNORED) {
                ctx.watch = -1;
            } else {
                updated = true;
            }
            pos += sizeof(struct inotify_event) + event->len;
        }
        if (updated) {
            viewer_reset();
        }
    }
}
#endif // HAVE_INOTIFY

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

void image_list_init(void)
{
    // register configuration loader
    config_add_loader(IMGLIST_CFG_SECTION, load_config);

#ifdef HAVE_INOTIFY
    ctx.notify = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (ctx.notify >= 0) {
        ui_add_event(ctx.notify, on_notify);
    }
#endif // HAVE_INOTIFY
}

bool image_list_scan(const char** files, size_t num)
{
    const char* force_start = NULL;

    if (num == 0) {
        // no input files specified, use all from the current directory
        add_dir(".", ctx.recursive);
    } else if (num == 1 && strcmp(files[0], "-") == 0) {
        // pipe mode
        add_file(STDIN_FILE_NAME);
        force_start = STDIN_FILE_NAME;
    } else {
        for (size_t i = 0; i < num; ++i) {
            struct stat file_stat;
            if (stat(files[i], &file_stat) == -1) {
                fprintf(stderr, "%s: [%i] %s\n", files[i], errno,
                        strerror(errno));
            } else {
                if (S_ISDIR(file_stat.st_mode)) {
                    add_dir(files[i], ctx.recursive);
                } else {
                    if (!ctx.all_files) {
                        add_file(files[i]);
                    } else {
                        // add all files from the same directory
                        const char* delim = strrchr(files[i], '/');
                        const size_t len = delim ? delim - files[i] : 0;
                        if (len == 0) {
                            add_dir(".", ctx.recursive);
                        } else {
                            char* dir = malloc(len + 1);
                            if (dir) {
                                memcpy(dir, files[i], len);
                                dir[len] = 0;
                                add_dir(dir, ctx.recursive);
                                free(dir);
                            }
                        }
                        if (!force_start) {
                            force_start = files[i];
                        }
                    }
                }
            }
        }
    }

    if (ctx.size == 0) {
        // empty list
        image_list_free();
        return false;
    }

    // sort or shuffle
    if (ctx.order == order_alpha) {
        sort_list();
    } else if (ctx.order == order_random) {
        shuffle_list();
    }

    // set initial index
    if (!force_start) {
        ctx.index = 0;
    } else {
        if (force_start[0] == '.' && force_start[1] == '/') {
            force_start += 2;
        }
        for (size_t i = 0; i < ctx.size; ++i) {
            if (strcmp(ctx.entries[i]->path, force_start) == 0) {
                ctx.index = i;
                break;
            }
        }
    }

    // load the first image
    if (strcmp(ctx.entries[ctx.index]->path, STDIN_FILE_NAME) == 0) {
        ctx.current = image_from_stdin();
    } else {
        ctx.current = image_from_file(ctx.entries[ctx.index]->path);
    }
    if (!ctx.current &&
        ((force_start && num == 1) || !image_list_jump(jump_next_file))) {
        image_list_free();
        return false;
    }

    preloader_ctl(true);

#ifdef HAVE_INOTIFY
    if (ctx.watch == -1) {
        watch_current();
    }
#endif // HAVE_INOTIFY

    return true;
}

void image_list_free(void)
{
    preloader_ctl(false);
    for (size_t i = 0; i < ctx.size; ++i) {
        free(ctx.entries[i]);
    }
    free(ctx.entries);
    image_free(ctx.prev);
    image_free(ctx.current);
    image_free(ctx.next);
    memset(&ctx, 0, sizeof(ctx));
}

size_t image_list_size(void)
{
    return ctx.size;
}

struct image_entry image_list_current(void)
{
    struct image_entry entry = { .index = ctx.index, .image = ctx.current };
    return entry;
}

bool image_list_skip(void)
{
    // remove current entry from list
    free(ctx.entries[ctx.index]);
    ctx.entries[ctx.index] = NULL;

    // open next image
    return image_list_jump(jump_next_file);
}

bool image_list_reset(void)
{
    // reset cache
    preloader_ctl(false);
    if (ctx.prev) {
        image_free(ctx.prev);
        ctx.prev = NULL;
    }
    if (ctx.next) {
        image_free(ctx.next);
        ctx.next = NULL;
    }

    // reload current image
    image_free(ctx.current);
    ctx.current = image_from_file(ctx.entries[ctx.index]->path);
    if (ctx.current) {
        preloader_ctl(true);
        return true;
    }

    // open nearest image
    return image_list_jump(jump_next_file) || image_list_jump(jump_prev_file);
}

bool image_list_jump(enum list_jump jump)
{
    struct image* image = NULL;
    size_t index = ctx.index;

    preloader_ctl(false);

    while (!image) {
        switch (jump) {
            case jump_first_file:
                index = peek_edge(true);
                break;
            case jump_last_file:
                index = peek_edge(false);
                break;
            case jump_next_file:
                index = peek_next_file(index, true);
                break;
            case jump_prev_file:
                index = peek_next_file(index, false);
                break;
            case jump_next_dir:
                index =
                    peek_next_dir(ctx.entries[ctx.index]->path, index, true);
                break;
            case jump_prev_dir:
                index =
                    peek_next_dir(ctx.entries[ctx.index]->path, index, false);
                break;
        }
        if (index == SIZE_MAX) {
            return false;
        }
        if (ctx.next && ctx.next->file_path == ctx.entries[index]->path) {
            image = ctx.next;
            ctx.next = NULL;
        } else if (ctx.prev &&
                   ctx.prev->file_path == ctx.entries[index]->path) {
            image = ctx.prev;
            ctx.prev = NULL;
        } else {
            image = image_from_file(ctx.entries[index]->path);
            if (!image) {
                // not an image, remove entry from list
                free(ctx.entries[index]);
                ctx.entries[index] = NULL;
            }
        }
    }

    image_free(ctx.prev);
    ctx.prev = ctx.current;
    ctx.current = image;
    ctx.index = index;

    preloader_ctl(true);

#ifdef HAVE_INOTIFY
    watch_current();
#endif // HAVE_INOTIFY

    return true;
}
