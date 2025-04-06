// SPDX-License-Identifier: MIT
// List of images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "imglist.h"

#include "application.h"
#include "array.h"
#include "buildcfg.h"

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef HAVE_INOTIFY
#include <sys/inotify.h>
#endif

/** Order of file list. */
enum list_order {
    order_none,    ///< Unsorted (system depended)
    order_alpha,   ///< Lexicographic sort
    order_numeric, ///< Numeric sort
    order_mtime,   ///< Modification time sort
    order_size,    ///< Size sort
    order_random   ///< Random order
};

// clang-format off
/** Order names. */
static const char* order_names[] = {
    [order_none] = "none",
    [order_alpha] = "alpha",
    [order_numeric] = "numeric",
    [order_mtime] = "mtime",
    [order_size] = "size",
    [order_random] = "random",
};
// clang-format on

/** Context of the image list. */
struct image_list {
    struct image* images; ///< Image list
    size_t size;          ///< Size of image list
    pthread_mutex_t lock; ///< List lock

    enum list_order order; ///< File list order
    bool reverse;          ///< Reverse order flag
    bool loop;             ///< File list loop mode
    bool recursive;        ///< Read directories recursively
    bool all_files;        ///< Open all files from the same directory

#ifdef HAVE_INOTIFY
    int notify;                ///< inotify file descriptor
    int watch;                 ///< Watch file descriptor
    imglist_watch_cb callback; ///< Watcher callback
#endif
};

/** Global image list instance. */
static struct image_list ctx;

/**
 * Get absolute path from relative source.
 * @param source relative file path
 * @param path output absolute path buffer
 * @param path_max size of the buffer
 * @return length of the absolute path without last null
 */
static size_t absolute_path(const char* source, char* path, size_t path_max)
{
    char buffer[PATH_MAX];
    struct str_slice dirs[1024];
    size_t dirs_num;
    size_t pos;

    if (strcmp(source, LDRSRC_STDIN) == 0 ||
        strncmp(source, LDRSRC_EXEC, LDRSRC_EXEC_LEN) == 0) {
        strncpy(path, source, path_max - 1);
        return strlen(path);
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
    path[pos] = 0;

    return pos;
}

/**
 * Search the right place to insert new entry according to sort order.
 * @param img new image entry to insert
 * @return image entry in the list that should be used as "before" position
 */
static struct image* ordered_position(const struct image* img)
{
    struct image* pos = NULL;

    if (ctx.order == order_none) {
        // unsorted
    } else if (ctx.order == order_random) {
        size_t index = rand() % ctx.size;
        list_for_each(ctx.images, struct image, it) {
            if (!index--) {
                pos = it;
                break;
            }
        }
    } else {
        list_for_each(ctx.images, struct image, it) {
            ssize_t cmp = 0;
            switch (ctx.order) {
                case order_alpha:
                    cmp = strcoll(img->source, it->source);
                    break;
                case order_numeric: {
                    const char* a = img->source;
                    const char* b = it->source;
                    while (cmp == 0 && *a && *b) {
                        if (isdigit(*a) && isdigit(*b)) {
                            cmp = strtoull(a, (char**)&a, 10) -
                                strtoull(b, (char**)&b, 10);
                        } else {
                            cmp = *a - *b;
                            ++a;
                            ++b;
                        }
                    }
                } break;
                case order_mtime:
                    cmp = it->file_time - img->file_time;
                    break;
                case order_size:
                    cmp = it->file_size - img->file_size;
                    break;
                case order_none:
                case order_random:
                    assert(false && "unreachable code");
                    break;
            }
            if ((ctx.reverse && cmp > 0) || (!ctx.reverse && cmp < 0)) {
                pos = it;
                break;
            }
        }
    }

    return pos;
}

/**
 * Add new entry to the list.
 * @param source image data source to add
 * @param st file stat (can be NULL)
 * @return created image entry
 */
static struct image* add_entry(const char* source, const struct stat* st)
{
    struct image* entry;
    struct image* pos;

    // search for duplicates
    entry = imglist_find(source);
    if (entry) {
        return entry;
    }

    // create new entry
    entry = image_create(source);
    if (!entry) {
        return NULL;
    }
    if (st) {
        entry->file_size = st->st_size;
        entry->file_time = st->st_mtime;
    }
    entry->index = ++ctx.size;

    // add entry to the list
    pos = ordered_position(entry);
    if (pos) {
        ctx.images = list_insert(pos, entry);
    } else {
        ctx.images = list_append(ctx.images, entry);
    }

    return entry;
}

/**
 * Add files from the directory to the list.
 * @param path absolute path to the directory
 * @return the first image entry in the directory
 */
static struct image* add_dir(const char* path)
{
    assert(path && path[strlen(path) - 1] == '/');

    struct image* img = NULL;
    char* full_path = NULL;
    struct dirent* dir_entry;
    DIR* dir_handle;

    dir_handle = opendir(path);
    if (!dir_handle) {
        return NULL;
    }
    while ((dir_entry = readdir(dir_handle))) {
        const char* name = dir_entry->d_name;
        struct stat st;

        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue; // skip link to self/parent
        }

        // compose full path
        if (!str_dup(path, &full_path) || !str_append(name, 0, &full_path)) {
            continue;
        }

        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                if (ctx.recursive && str_append("/", 1, &full_path)) {
                    img = add_dir(full_path);
                }
            } else if (S_ISREG(st.st_mode)) {
                img = add_entry(full_path, &st);
            }
        }
    }

    // get the first image in the directory
    if (img) {
        const size_t plen = strlen(path);
        list_for_each_back(img, struct image, it) {
            if (strncmp(path, it->source, plen) != 0) {
                img = list_is_last(it) ? it : list_next(it);
                break;
            }
        }
    }

    free(full_path);
    closedir(dir_handle);

    return img;
}

/**
 * Add image source to the list.
 * @param source image source to add (file path or special prefix)
 * @return created image entry or NULL on errors or if source is directory
 */
static struct image* add_source(const char* source)
{
    struct stat st;
    char fspath[PATH_MAX];

    // special url
    if (strncmp(source, LDRSRC_STDIN, LDRSRC_STDIN_LEN) == 0 ||
        strncmp(source, LDRSRC_EXEC, LDRSRC_EXEC_LEN) == 0) {
        return add_entry(source, NULL);
    }

    // file from file system
    if (stat(source, &st) != 0) {
        const int rc = errno;
        fprintf(stderr, "Ignore file %s: [%i] %s\n", source, rc, strerror(rc));
        return NULL;
    }

    // get absolute path
    if (!absolute_path(source, fspath, sizeof(fspath))) {
        fprintf(stderr, "Ignore file %s: unknown absolute path\n", source);
        return NULL;
    }

    // add directory to the list
    if (S_ISDIR(st.st_mode)) {
        const size_t len = strlen(fspath);
        if (fspath[len - 1] != '/' && len + 1 < sizeof(fspath)) {
            // append slash
            fspath[len] = '/';
            fspath[len + 1] = 0;
        }
        return add_dir(fspath);
    }

    // add file to the list
    if (S_ISREG(st.st_mode)) {
        return add_entry(fspath, &st);
    }

    fprintf(stderr, "Ignore special file %s\n", source);
    return NULL;
}

/** Reindex the image list. */
static void reindex(void)
{
    ctx.size = 0;
    list_for_each(ctx.images, struct image, it) {
        it->index = ++ctx.size;
    }
}

/**
 * Get next image with different parent (parent dir).
 * @param img start entry
 * @param loop enable/disable loop mode
 * @param forward direction (forward/backward)
 * @return image instance or NULL if not found
 */
static struct image* get_next_parent(struct image* img, bool loop, bool forward)
{
    const char* cur_src = img->source;
    const char* cur_delim = strrchr(cur_src, '/');
    const size_t cur_len = cur_delim ? cur_delim - cur_src : 0;
    struct image* next = NULL;
    struct image* it = img;

    while (!next) {
        const char* it_src;
        const char* it_delim;
        size_t it_len;

        if (forward) {
            it = list_next(it);
            if (!it && loop) {
                it = ctx.images;
            }
        } else {
            it = list_prev(it);
            if (!it && loop) {
                it = list_get_last(ctx.images);
            }
        }
        if (!it || it == img) {
            break;
        }

        it_src = it->source;
        it_delim = strrchr(it_src, '/');
        it_len = it_delim ? it_delim - it_src : 0;

        if (cur_len != it_len || strncmp(cur_src, it_src, cur_len) != 0) {
            next = it;
        }
    }

    return next;
}

#ifdef HAVE_INOTIFY
/** inotify handler. */
static void on_inotify(__attribute__((unused)) void* data)
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
            ctx.callback();
        }
    }
}
#endif // HAVE_INOTIFY

void imglist_init(const struct config* cfg)
{
    pthread_mutex_init(&ctx.lock, NULL);

    ctx.order = config_get_oneof(cfg, CFG_LIST, CFG_LIST_ORDER, order_names,
                                 ARRAY_SIZE(order_names));
    ctx.reverse = config_get_bool(cfg, CFG_LIST, CFG_LIST_REVERSE);
    ctx.loop = config_get_bool(cfg, CFG_LIST, CFG_LIST_LOOP);
    ctx.recursive = config_get_bool(cfg, CFG_LIST, CFG_LIST_RECURSIVE);
    ctx.all_files = config_get_bool(cfg, CFG_LIST, CFG_LIST_ALL);

#ifdef HAVE_INOTIFY
    ctx.watch = -1;
    ctx.notify = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (ctx.notify >= 0) {
        app_watch(ctx.notify, on_inotify, NULL);
    }
#endif // HAVE_INOTIFY
}

void imglist_destroy(void)
{
#ifdef HAVE_INOTIFY
    if (ctx.notify >= 0) {
        if (ctx.watch != -1) {
            inotify_rm_watch(ctx.notify, ctx.watch);
        }
        close(ctx.notify);
    }
#endif // HAVE_INOTIFY

    list_for_each(ctx.images, struct image, it) {
        image_free(it, IMGFREE_ALL);
    }

    ctx.images = NULL;
    ctx.size = 0;

    pthread_mutex_destroy(&ctx.lock);
}

void imglist_lock(void)
{
    pthread_mutex_lock(&ctx.lock);
}

void imglist_unlock(void)
{
    pthread_mutex_unlock(&ctx.lock);
}

struct image* imglist_load(const char* const* sources, size_t num)
{
    assert(ctx.size == 0);

    struct image* img = NULL;

    // compose image list
    if (num == 0) {
        // no input files specified, use all from the current directory
        img = add_source(".");
    } else if (num == 1) {
        if (strcmp(sources[0], "-") == 0) {
            img = add_source(LDRSRC_STDIN);
        } else {
            img = add_source(sources[0]);
            if (img && ctx.size == 1 && ctx.all_files) {
                // add neighbors (all files from the same directory)
                const char* delim = strrchr(img->source, '/');
                if (delim) {
                    const size_t len = delim - img->source + 1 /* last slash */;
                    char* dir = str_append(img->source, len, NULL);
                    if (dir) {
                        add_dir(dir);
                        free(dir);
                    }
                }
            }
        }
    } else {
        for (size_t i = 0; i < num; ++i) {
            struct image* added = add_source(sources[i]);
            if (!img && added) {
                img = added;
            }
        }
    }

    return img;
}

void imglist_remove(struct image* img)
{
    ctx.images = list_remove(img);
    image_free(img, IMGFREE_ALL);
    reindex();
}

struct image* imglist_find(const char* source)
{
    list_for_each(ctx.images, struct image, it) {
        if (strcmp(source, it->source) == 0) {
            return it;
        }
    }
    return NULL;
}

size_t imglist_size(void)
{
    return ctx.size;
}

struct image* imglist_first(void)
{
    return ctx.images;
}

struct image* imglist_last(void)
{
    return list_get_last(ctx.images);
}

struct image* imglist_next(struct image* img)
{
    return list_next(img);
}

struct image* imglist_prev(struct image* img)
{
    return list_prev(img);
}

struct image* imglist_next_file(struct image* img)
{
    struct image* next = imglist_next(img);

    if (!next && ctx.loop) {
        next = ctx.images;
        if (next) {
            if (next == img) {
                next = NULL;
            }
        }
    }

    return next;
}

struct image* imglist_prev_file(struct image* img)
{
    struct image* prev = imglist_prev(img);

    if (!prev && ctx.loop) {
        prev = list_get_last(ctx.images);
        if (prev) {
            if (prev == img) {
                prev = NULL;
            }
        }
    }

    return prev;
}

struct image* imglist_next_dir(struct image* img)
{
    return get_next_parent(img, ctx.loop, true);
}

struct image* imglist_prev_dir(struct image* img)
{
    return get_next_parent(img, ctx.loop, false);
}

struct image* imglist_rand(struct image* img)
{
    size_t offset = 1 + rand() % (ctx.size - 1);

    while (offset--) {
        img = list_next(img);
        if (!img) {
            img = ctx.images;
        }
    }

    return img;
}

struct image* imglist_jump(struct image* img, ssize_t distance)
{
    struct image* target = NULL;

    if (distance > 0) {
        list_for_each(img, struct image, it) {
            if (distance-- == 0) {
                target = it;
                break;
            }
        }
    } else if (distance < 0) {
        list_for_each_back(img, struct image, it) {
            if (distance++ == 0) {
                target = it;
                break;
            }
        }
    } else {
        target = img;
    }

    return target;
}

ssize_t imglist_distance(const struct image* start, const struct image* end)
{
    ssize_t distance = 0;

    if (start->index <= end->index) {
        list_for_each(start, const struct image, it) {
            if (it == end) {
                break;
            }
            ++distance;
        }
    } else {
        list_for_each(end, const struct image, it) {
            if (it == start) {
                break;
            }
            --distance;
        }
    }

    return distance;
}

void imglist_watch(struct image* img, imglist_watch_cb callback)
{
#ifdef HAVE_INOTIFY
    // register inotify watcher
    if (ctx.notify >= 0) {
        if (ctx.watch != -1) {
            inotify_rm_watch(ctx.notify, ctx.watch);
            ctx.watch = -1;
        }
        if (img) {
            ctx.callback = callback;
            ctx.watch =
                inotify_add_watch(ctx.notify, img->source,
                                  IN_MODIFY | IN_MOVE_SELF | IN_DELETE_SELF);
        }
    }
#else
    (void)img;
    (void)callback;
#endif
}
