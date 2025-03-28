// SPDX-License-Identifier: MIT
// List of images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "imagelist.h"

#include "application.h"
#include "array.h"
#include "buildcfg.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_INOTIFY
#include <sys/inotify.h>
#endif

/** Order of file list. */
enum list_order {
    order_none,  ///< Unsorted (system depended)
    order_alpha, ///< Alphanumeric sort
    order_mtime, ///< Modification time sort
    order_size,  ///< Size sort
    order_random ///< Random order
};

// clang-format off
/** Order names. */
static const char* order_names[] = {
    [order_none] = "none",
    [order_alpha] = "alpha",
    [order_mtime] = "mtime",
    [order_size] = "size",
    [order_random] = "random",
};
// clang-format on

/** Context of the image list. */
struct image_list {
    struct image* images;  ///< Image list
    size_t size;           ///< Size of image list
    pthread_rwlock_t lock; ///< List RW lock

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
 * @return length of the absolute path including last null
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
 * @param st file stat (can be NULL)
 * @return created image entry
 */
static struct image* add_entry(const char* source, const struct stat* st)
{
    struct image* entry;
    struct image* pos;

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

    pthread_rwlock_wrlock(&ctx.lock);

    // seach the right place to insert according to sort order
    pos = NULL;
    if (ctx.order == order_alpha || ctx.order == order_mtime ||
        ctx.order == order_size) {
        list_for_each(ctx.images, struct image, it) {
            ssize_t cmp = 0;
            switch (ctx.order) {
                case order_alpha:
                    cmp = strcoll(source, it->source);
                    break;
                case order_mtime:
                    cmp = it->file_time - entry->file_time;
                    break;
                case order_size:
                    cmp = it->file_size - entry->file_size;
                    break;
                default:
                    break;
            }
            if ((ctx.reverse && cmp > 0) || (!ctx.reverse && cmp < 0)) {
                pos = it;
                break;
            }
        }
    } else if (ctx.order == order_random) {
        size_t index = rand() % ctx.size;
        list_for_each(ctx.images, struct image, it) {
            if (!index--) {
                pos = it;
                break;
            }
        }
    }

    // add entry to the list
    if (pos) {
        ctx.images = list_insert(pos, entry);
    } else {
        ctx.images = list_append(ctx.images, entry);
    }

    pthread_rwlock_unlock(&ctx.lock);

    return entry;
}

/**
 * Add file to the list.
 * @param file path to the file
 * @param st file stat
 * @return created image entry
 */
static struct image* add_file(const char* path, const struct stat* st)
{
    char abspath[PATH_MAX];
    if (absolute_path(path, abspath, sizeof(abspath))) {
        return add_entry(abspath, st);
    }
    return NULL;
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

    pthread_rwlock_rdlock(&ctx.lock);

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
            image_addref(next);
        }
    }

    pthread_rwlock_unlock(&ctx.lock);

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
    pthread_rwlock_init(&ctx.lock, NULL);

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

    pthread_rwlock_wrlock(&ctx.lock);
    list_for_each(ctx.images, struct image, it) {
        assert(it->ref_count == 1); // should be the last owner at this moment
        image_deref(it);
    }
    pthread_rwlock_unlock(&ctx.lock);

    ctx.images = NULL;
    ctx.size = 0;

    pthread_rwlock_destroy(&ctx.lock);
}

struct image* imglist_add(const char* source)
{
    struct stat st;

    // special url
    if (strncmp(source, LDRSRC_STDIN, LDRSRC_STDIN_LEN) == 0 ||
        strncmp(source, LDRSRC_EXEC, LDRSRC_EXEC_LEN) == 0) {
        return add_entry(source, NULL);
    }

    // file from file system
    if (stat(source, &st) != 0) {
        fprintf(stderr, "Unable to query file %s: [%i] %s\n", source, errno,
                strerror(errno));
        return NULL;
    }
    if (S_ISDIR(st.st_mode)) {
        add_dir(source);
    } else if (S_ISREG(st.st_mode)) {
        if (!ctx.all_files) {
            return add_file(source, &st);
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
    } else {
        fprintf(stderr, "Unable to open special file %s\n", source);
    }

    return NULL;
}

void imglist_remove(struct image* img)
{
    pthread_rwlock_wrlock(&ctx.lock);
    ctx.images = list_remove(img);
    image_deref(img);
    pthread_rwlock_unlock(&ctx.lock);

    imglist_reindex();
}

void imglist_reindex(void)
{
    ctx.size = 0;
    pthread_rwlock_rdlock(&ctx.lock);
    list_for_each(ctx.images, struct image, it) {
        it->index = ++ctx.size;
    }
    pthread_rwlock_unlock(&ctx.lock);
}

size_t imglist_size(void)
{
    return ctx.size;
}

struct image* imglist_first(void)
{
    struct image* first;

    pthread_rwlock_rdlock(&ctx.lock);
    first = ctx.images;
    if (first) {
        image_addref(first);
    }
    pthread_rwlock_unlock(&ctx.lock);

    return first;
}

struct image* imglist_last(void)
{
    struct image* last;

    pthread_rwlock_rdlock(&ctx.lock);
    last = list_get_last(ctx.images);
    if (last) {
        image_addref(last);
    }
    pthread_rwlock_unlock(&ctx.lock);

    return last;
}

struct image* imglist_next(struct image* img)
{
    struct image* next;

    pthread_rwlock_rdlock(&ctx.lock);
    next = list_next(img);
    if (next) {
        image_addref(next);
    }
    pthread_rwlock_unlock(&ctx.lock);

    return next;
}

struct image* imglist_prev(struct image* img)
{
    struct image* prev;

    pthread_rwlock_rdlock(&ctx.lock);
    prev = list_prev(img);
    if (prev) {
        image_addref(prev);
    }
    pthread_rwlock_unlock(&ctx.lock);

    return prev;
}

struct image* imglist_next_file(struct image* img)
{
    struct image* next = imglist_next(img);

    if (!next && ctx.loop) {
        pthread_rwlock_rdlock(&ctx.lock);
        next = ctx.images;
        if (next) {
            if (next == img) {
                next = NULL;
            } else {
                image_addref(next);
            }
        }
        pthread_rwlock_unlock(&ctx.lock);
    }

    return next;
}

struct image* imglist_prev_file(struct image* img)
{
    struct image* prev = imglist_prev(img);

    if (!prev && ctx.loop) {
        pthread_rwlock_rdlock(&ctx.lock);
        prev = list_get_last(ctx.images);
        if (prev) {
            if (prev == img) {
                prev = NULL;
            } else {
                image_addref(prev);
            }
        }
        pthread_rwlock_unlock(&ctx.lock);
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

    pthread_rwlock_rdlock(&ctx.lock);
    while (offset--) {
        img = list_next(img);
        if (!img) {
            img = ctx.images;
        }
    }
    if (img) {
        image_addref(img);
    }
    pthread_rwlock_unlock(&ctx.lock);

    return img;
}

struct image* imglist_fwd(struct image* img, size_t distance)
{
    struct image* next = NULL;

    pthread_rwlock_rdlock(&ctx.lock);
    list_for_each(img, struct image, it) {
        if (list_is_last(it) || distance-- == 0) {
            next = it;
            image_addref(next);
            break;
        }
    }
    pthread_rwlock_unlock(&ctx.lock);

    return next;
}

struct image* imglist_back(struct image* img, size_t distance)
{
    struct image* prev = NULL;

    pthread_rwlock_rdlock(&ctx.lock);
    list_for_each_back(img, struct image, it) {
        if (list_is_first(it) || distance-- == 0) {
            prev = it;
            image_addref(prev);
            break;
        }
    }
    pthread_rwlock_unlock(&ctx.lock);

    return prev;
}

size_t imglist_distance(const struct image* start, const struct image* end)
{
    size_t distance = 0;

    if (start->index > end->index) {
        const struct image* swap = start;
        start = end;
        end = swap;
    }

    pthread_rwlock_rdlock(&ctx.lock);
    list_for_each(start, const struct image, it) {
        if (it == end) {
            break;
        }
        ++distance;
    }
    pthread_rwlock_unlock(&ctx.lock);

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
