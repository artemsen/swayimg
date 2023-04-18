// SPDX-License-Identifier: MIT
// List of images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "imagelist.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

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
    struct image* prev;     ///< Prevous image handle (cached)
    struct image* current;  ///< Current image handle
    struct image* next;     ///< Next image handle (preload)
    pthread_t preloader;    ///< Preload thread
    const struct config* config; ///< Configuration
};

/**
 * Add file to the list.
 * @param ctx image list context
 * @param file path to the file
 */
static void add_file(struct image_list* ctx, const char* file)
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
    for (size_t i = 0; i < ctx->size; ++i) {
        if (strcmp(ctx->entries[i]->path, file) == 0) {
            return;
        }
    }

    // relocate array, if needed
    if (ctx->index >= ctx->alloc) {
        const size_t num = ctx->alloc + ALLOCATE_SIZE;
        struct entry** ptr = realloc(ctx->entries, num * sizeof(struct entry*));
        if (!ptr) {
            return;
        }
        ctx->alloc = num;
        ctx->entries = ptr;
    }

    // add new entry
    entry_sz = sizeof(struct entry) + path_len;
    entry = malloc(entry_sz);
    if (entry) {
        memcpy(entry->path, file, path_len + 1);
        entry->index = ctx->index;
        ctx->entries[ctx->index] = entry;
        ++ctx->index;
        ++ctx->size;
    }
}

/**
 * Add files from the directory to the list.
 * @param ctx image list context
 * @param dir full path to the directory
 * @param recursive flag to handle directory recursively
 */
static void add_dir(struct image_list* ctx, const char* dir, bool recursive)
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
 * Peek next file entry.
 * @param ctx image list context
 * @param start index of the start position
 * @param forward step direction
 * @return index of the next entry or SIZE_MAX if not found
 */
static size_t peek_next_file(const struct image_list* ctx, size_t start,
                             bool forward)
{
    size_t index = start;

    while (true) {
        if (forward) {
            if (++index >= ctx->size) {
                if (!ctx->config->loop) {
                    break;
                }
                index = 0;
            }
        } else {
            if (index-- == 0) {
                if (!ctx->config->loop) {
                    break;
                }
                index = ctx->size - 1;
            }
        }

        if (index == start) {
            break;
        }

        if (ctx->entries[index]) {
            return index;
        }
    }

    return SIZE_MAX;
}

/**
 * Peek next directory entry.
 * @param ctx image list context
 * @param file path to extarct source directory
 * @param start index of the start position
 * @param forward step direction
 * @return index of the next entry or SIZE_MAX if not found
 */
static size_t peek_next_dir(const struct image_list* ctx, const char* file,
                            size_t start, bool forward)
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

        index = peek_next_file(ctx, index, forward);
        if (index == SIZE_MAX) {
            break; // not found
        }

        next_path = ctx->entries[index]->path;
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
 * @param ctx image list context
 * @param first direction, true=first, false=last
 * @return index of the next entry or SIZE_MAX if not found
 */
static size_t peek_edge(const struct image_list* ctx, bool first)
{
    size_t index = first ? 0 : ctx->size - 1;
    if (index == ctx->index || ctx->entries[index]) {
        return index;
    }
    return peek_next_file(ctx, ctx->index, first);
}

/**
 * Sort the image list alphabetically.
 * @param ctx image list context
 */
static void sort_list(struct image_list* ctx)
{
    for (size_t i = 0; i < ctx->size; ++i) {
        for (size_t j = i + 1; j < ctx->size; ++j) {
            if (strcoll(ctx->entries[i]->path, ctx->entries[j]->path) > 0) {
                struct entry* swap = ctx->entries[i];
                ctx->entries[i] = ctx->entries[j];
                ctx->entries[j] = swap;
            }
        }
        ctx->entries[i]->index = i;
    }
}

/**
 * Shuffle the image list.
 * @param ctx image list context
 */
static void shuffle_list(struct image_list* ctx)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    srand(ts.tv_nsec);

    // swap random entries
    for (size_t i = 0; i < ctx->size; ++i) {
        const size_t j = rand() % ctx->size;
        if (i != j) {
            struct entry* swap = ctx->entries[i];
            ctx->entries[i] = ctx->entries[j];
            ctx->entries[j] = swap;
        }
    }
}

/** Image preloader executed in background thread. */
static void* preloader_thread(void* data)
{
    struct image_list* ctx = data;
    size_t index = ctx->index;

    while (true) {
        struct image* img;

        index = peek_next_file(ctx, index, true);
        if (index == SIZE_MAX) {
            break; // next image not found
        }
        if ((ctx->next && ctx->next->file_path == ctx->entries[index]->path) ||
            (ctx->prev && ctx->prev->file_path == ctx->entries[index]->path)) {
            break; // already loaded
        }

        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

        img = image_from_file(ctx->entries[index]->path);
        if (img) {
            image_free(ctx->next);
            ctx->next = img;
            break;
        } else {
            // not an image, remove entry from list
            free(ctx->entries[index]);
            ctx->entries[index] = NULL;
        }

        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    }

    return NULL;
}

/**
 * Stop or restart background thread to preload adjacent images.
 * @param ctx image list context
 * @param restart action: true=restart, false=stop preloader
 */
static void preloader_ctl(struct image_list* ctx, bool restart)
{
    if (ctx->preloader) {
        pthread_cancel(ctx->preloader);
        pthread_join(ctx->preloader, NULL);
        ctx->preloader = 0;
    }
    if (restart) {
        pthread_create(&ctx->preloader, NULL, preloader_thread, ctx);
    }
}

struct image_list* image_list_init(const char** files, size_t num,
                                   const struct config* cfg)
{
    struct image_list* ctx;
    const char* force_start = NULL;

    ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        fprintf(stderr, "Not enough memory\n");
        return NULL;
    }
    ctx->config = cfg;

    if (num == 0) {
        // no input files specified, use all from the current directory
        add_dir(ctx, ".", cfg->recursive);
    } else if (num == 1 && strcmp(files[0], "-") == 0) {
        // pipe mode
        add_file(ctx, STDIN_FILE_NAME);
        force_start = STDIN_FILE_NAME;
    } else {
        for (size_t i = 0; i < num; ++i) {
            struct stat file_stat;
            if (strncmp(files[i], "http://", 7) == 0) {
                add_file(ctx, files[i]);
            } else if (strncmp(files[i], "https://", 8) == 0) {
                add_file(ctx, files[i]);
            } else if (stat(files[i], &file_stat) == -1) {
                fprintf(stderr, "%s: [%i] %s\n", files[i], errno,
                        strerror(errno));
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
                    }
                }
            }
        }
    }

    if (ctx->size == 0) {
        // empty list
        image_list_free(ctx);
        return NULL;
    }

    // sort or shuffle
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
            if (strcmp(ctx->entries[i]->path, force_start) == 0) {
                ctx->index = i;
                break;
            }
        }
    }

    // load the first image
    if (strcmp(ctx->entries[ctx->index]->path, STDIN_FILE_NAME) == 0) {
        ctx->current = image_from_stdin();
    } else {
        ctx->current = image_from_file(ctx->entries[ctx->index]->path);
    }
    if (!ctx->current &&
        ((force_start && num == 1) || !image_list_jump(ctx, jump_next_file))) {
        image_list_free(ctx);
        return NULL;
    }

    preloader_ctl(ctx, true);

    return ctx;
}

void image_list_free(struct image_list* ctx)
{
    if (ctx) {
        preloader_ctl(ctx, false);
        for (size_t i = 0; i < ctx->size; ++i) {
            free(ctx->entries[i]);
        }
        free(ctx->entries);
        image_free(ctx->prev);
        image_free(ctx->current);
        image_free(ctx->next);
        free(ctx);
    }
}

size_t image_list_size(const struct image_list* ctx)
{
    return ctx->size;
}

struct image_entry image_list_current(const struct image_list* ctx)
{
    struct image_entry entry = { .index = ctx->index, .image = ctx->current };
    return entry;
}

int image_list_cur_exec(const struct image_list* ctx)
{
    const char* template = ctx->config->exec_cmd;
    const char* path = ctx->current->file_path;
    size_t pos = 0;
    size_t len = 0;
    char* cmd = NULL;
    int rc = -1;

    // construct command text
    while (*template) {
        const char* append = template;
        size_t append_sz = 1;
        if (*template == '%') {
            if (*(template + 1) == '%') {
                // escaped %
                ++template;
            } else {
                // replace % with path
                append = path;
                append_sz = strlen(path);
            }
        }
        ++template;
        if (pos + append_sz >= len) {
            char* ptr;
            len = pos + append_sz + 32;
            ptr = realloc(cmd, len);
            if (!ptr) {
                fprintf(stderr, "Not enough memory\n");
                free(cmd);
                cmd = NULL;
                break;
            }
            cmd = ptr;
        }
        memcpy(cmd + pos, append, append_sz);
        pos += append_sz;
    }

    // execute command
    if (cmd) {
        cmd[pos] = 0;
        rc = system(cmd);
        free(cmd);
    }

    return rc;
}

bool image_list_cur_reload(struct image_list* ctx)
{
    struct image* image = image_from_file(ctx->current->file_path);
    if (image) {
        image_free(ctx->current);
        ctx->current = image;
    }
    return !!image;
}

bool image_list_jump(struct image_list* ctx, enum list_jump jump)
{
    struct image* image = NULL;
    size_t index = ctx->index;

    preloader_ctl(ctx, false);

    while (!image) {
        switch (jump) {
            case jump_first_file:
                index = peek_edge(ctx, true);
                break;
            case jump_last_file:
                index = peek_edge(ctx, false);
                break;
            case jump_next_file:
                index = peek_next_file(ctx, index, true);
                break;
            case jump_prev_file:
                index = peek_next_file(ctx, index, false);
                break;
            case jump_next_dir:
                index = peek_next_dir(ctx, ctx->entries[ctx->index]->path,
                                      index, true);
                break;
            case jump_prev_dir:
                index = peek_next_dir(ctx, ctx->entries[ctx->index]->path,
                                      index, false);
                break;
        }
        if (index == SIZE_MAX) {
            return false;
        }
        if (ctx->next && ctx->next->file_path == ctx->entries[index]->path) {
            image = ctx->next;
            ctx->next = NULL;
        } else if (ctx->prev &&
                   ctx->prev->file_path == ctx->entries[index]->path) {
            image = ctx->prev;
            ctx->prev = NULL;
        } else {
            image = image_from_file(ctx->entries[index]->path);
            if (!image) {
                // not an image, remove entry from list
                free(ctx->entries[index]);
                ctx->entries[index] = NULL;
            }
        }
    }

    image_free(ctx->prev);
    ctx->prev = ctx->current;
    ctx->current = image;
    ctx->index = index;

    preloader_ctl(ctx, true);

    return true;
}
