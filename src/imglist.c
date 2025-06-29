// SPDX-License-Identifier: MIT
// List of images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "imglist.h"

#include "application.h"
#include "array.h"
#include "buildcfg.h"
#include "fs.h"

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
    bool recursive;        ///< Read directories recursively
    bool all_files;        ///< Open all files from the same directory
    bool from_file;        ///< Interpret input files as lists
};

/** Global image list instance. */
static struct image_list ctx;

/**
 * Numeric compare.
 * @param str0,str1 strings to compare
 * @return compare result
 */
static inline int compare_numeric(const char* str0, const char* str1)
{
    int rc = 0;

    while (rc == 0 && *str0 && *str1) {
        if (isdigit(*str0) && isdigit(*str1)) {
            char* end0;
            char* end1;
            rc = strtoull(str0, &end0, 10) - strtoull(str1, &end1, 10);
            str0 = end0;
            str1 = end1;
        } else {
            rc = *str0 - *str1;
            ++str0;
            ++str1;
        }
    }

    return rc;
}

/**
 * Compare two image instances.
 * @param ppi0,ppi1 pointers to pointers to image instances
 * @return compare result
 */
static int compare(const void* ppi0, const void* ppi1)
{
    const struct image* img0 = *(const struct image* const*)ppi0;
    const struct image* img1 = *(const struct image* const*)ppi1;
    const char* src0 = img0->source;
    const char* src1 = img1->source;
    const char* parent0 = strrchr(src0, '/');
    const char* parent1 = strrchr(src1, '/');
    const size_t plen0 = parent0 ? parent0 - src0 : 0;
    const size_t plen1 = parent1 ? parent1 - src1 : 0;
    char path0[PATH_MAX], path1[PATH_MAX];
    int rc = 0;

    // compare parent directories to prevent mix with files
    if (plen0 && plen0 < sizeof(path0) && plen1 && plen1 < sizeof(path1)) {
        memcpy(path0, src0, plen0);
        path0[plen0] = 0;
        memcpy(path1, src1, plen1);
        path1[plen1] = 0;

        if (ctx.order == order_numeric) {
            rc = compare_numeric(path0, path1);
        } else {
            rc = strcoll(path0, path1);
        }
        if (rc) {
            return ctx.reverse ? -rc : rc;
        }

        // skip parent from compare
        src0 += plen0;
        src1 += plen1;
    }

    switch (ctx.order) {
        case order_alpha:
            rc = strcoll(src0, src1);
            break;
        case order_numeric:
            rc = compare_numeric(src0, src1);
            break;
        case order_mtime:
            rc = img1->file_time - img0->file_time;
            break;
        case order_size:
            rc = img1->file_size - img0->file_size;
            break;
        case order_none:
        case order_random:
            assert(false && "cannot compare");
            break;
    }

    return ctx.reverse ? -rc : rc;
}

/**
 * Sort image list considering image list config.
 */
static void sort(void)
{
    struct image* last;
    struct array* arr;
    size_t i;

    // same as input or system dependent
    if (ctx.order == order_none) {
        if (!ctx.reverse) { // list is created in reverse order, reorder it
            list_for_each(ctx.images, struct image, it) {
                struct list* next = it->list.next;
                it->list.next = it->list.prev;
                it->list.prev = next;
                if (!next) { // last entry
                    ctx.images = it;
                }
            }
        }
        return;
    }

    // transform list to array with pointers
    arr = arr_create(ctx.size, sizeof(struct image*));
    if (!arr) {
        return;
    }
    i = 0;
    list_for_each(ctx.images, struct image, it) {
        *(struct image**)arr_nth(arr, i++) = it;
        it->list.next = NULL;
        it->list.prev = NULL;
    }

    if (ctx.order == order_random) {
        // shuffle
        for (i = 0; i < arr->size; ++i) {
            const size_t swap_index = rand() % arr->size;
            if (i != swap_index) {
                struct image** entry0 = arr_nth(arr, i);
                struct image** entry1 = arr_nth(arr, swap_index);
                struct image* tmp = *entry0;
                *entry0 = *entry1;
                *entry1 = tmp;
            }
        }
    } else if (ctx.order == order_alpha || ctx.order == order_numeric ||
               ctx.order == order_mtime || ctx.order == order_size) {
        // sort in specific order
        qsort(arr_nth(arr, 0), arr->size, arr->item_size, compare);
    }

    // transform array to list
    ctx.images = *(struct image**)arr_nth(arr, 0);
    last = ctx.images;
    for (i = 1; i < arr->size; ++i) {
        list_append(last, *(struct image**)arr_nth(arr, i));
        last = list_next(last);
    }

    arr_free(arr);
}

/**
 * Add new entry to the list.
 * @param source image data source to add
 * @param st file stat (can be NULL)
 * @param ordered flag to used ordered insert
 * @return created image entry
 */
static struct image* add_entry(const char* source, const struct stat* st,
                               bool ordered)
{
    struct image* entry;
    struct image* pos = NULL;

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

    // search the right place to insert new entry according to sort order
    if (ordered) {
        switch (ctx.order) {
            case order_none:
                break;
            case order_alpha:
            case order_numeric:
            case order_mtime:
            case order_size:
                list_for_each(ctx.images, struct image, it) {
                    if (compare(&entry, &it) > 0) {
                        pos = it;
                        break;
                    }
                }
                break;
            case order_random: {
                size_t index = rand() % ctx.size;
                list_for_each(ctx.images, struct image, it) {
                    if (!index--) {
                        pos = it;
                        break;
                    }
                }
            } break;
        }
    }

    // add entry to the list
    if (pos) {
        ctx.images = list_insert(pos, entry);
    } else {
        ctx.images = list_add(ctx.images, entry);
    }

    return entry;
}

/**
 * Add files from the directory to the list.
 * @param dir absolute path to the directory
 * @param ordered flag to used ordered insert
 * @return image entry in the directory
 */
static struct image* add_dir(const char* dir, bool ordered)
{
    struct image* first = NULL;
    struct image* subdir = NULL;
    struct dirent* dir_entry;
    DIR* dir_handle;

    dir_handle = opendir(dir);
    if (!dir_handle) {
        return NULL;
    }

    while ((dir_entry = readdir(dir_handle))) {
        char path[PATH_MAX] = { 0 };
        const char* name = dir_entry->d_name;
        struct stat st;

        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue; // skip link to self/parent
        }
        // compose full path
        strncpy(path, dir, sizeof(path) - 1);
        if (!fs_append_path(name, path, sizeof(path))) {
            continue; // buffer too small
        }

        if (stat(path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                if (ctx.recursive) {
                    struct image* added;
                    fs_append_path(NULL, path, sizeof(path)); // append slash
                    added = add_dir(path, ordered);
                    if (!first && added &&
                        (!subdir || compare(&added, &subdir) < 0)) {
                        subdir = added;
                    }
                }
            } else if (S_ISREG(st.st_mode)) {
                struct image* added = add_entry(path, &st, ordered);
                if (added &&
                    (!first ||
                     (ctx.order != order_none && ctx.order != order_random &&
                      compare(&added, &first) < 0))) {
                    first = added;
                }
            }
        }
    }

    fs_monitor_add(dir);

    closedir(dir_handle);

    return first ? first : subdir;
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
        return add_entry(source, NULL, false);
    }

    // file from file system
    if (stat(source, &st) != 0) {
        const int rc = errno;
        fprintf(stderr, "Ignore file %s: [%i] %s\n", source, rc, strerror(rc));
        return NULL;
    }

    // get absolute path
    if (!fs_abspath(source, fspath, sizeof(fspath))) {
        fprintf(stderr, "Ignore file %s: unknown absolute path\n", source);
        return NULL;
    }

    // add directory to the list
    if (S_ISDIR(st.st_mode)) {
        fs_append_path(NULL, fspath, sizeof(fspath)); // append slash
        return add_dir(fspath, false);
    }

    // add file to the list
    if (S_ISREG(st.st_mode)) {
        struct image* img = add_entry(fspath, &st, false);
        if (img && !ctx.all_files) {
            fs_monitor_add(img->source);
        }
        return img;
    }

    fprintf(stderr, "Ignore special file %s\n", source);
    return NULL;
}

/**
 * Construct image list from specified sources.
 * @param sources array of sources
 * @param num number of sources in the array
 * @return first image instance to show or NULL if list is empty
 */
static struct image* load_sources(const char* const* sources, size_t num)
{
    struct image* img = NULL;

    // compose image list
    if (num == 0) {
        // no input files specified, use all from the current directory
        img = add_source(".");
        ctx.all_files = false;
    } else if (num == 1) {
        if (strcmp(sources[0], "-") == 0) {
            img = add_source(LDRSRC_STDIN);
        } else {
            if (ctx.all_files) {
                // the "all files" mode is not applicable for directory
                struct stat st;
                if (stat(sources[0], &st) == 0 && S_ISDIR(st.st_mode)) {
                    ctx.all_files = false;
                }
            }
            img = add_source(sources[0]);
            if (img && ctx.all_files) {
                // add neighbors (all files from the same directory)
                const char* delim = strrchr(img->source, '/');
                if (delim) {
                    char dir[PATH_MAX] = { 0 };
                    const size_t len = delim - img->source + 1 /* last slash */;
                    if (len < sizeof(dir)) {
                        strncpy(dir, img->source, len);
                        add_dir(dir, false);
                    }
                }
            }
        }
    } else {
        ctx.all_files = false;
        for (size_t i = 0; i < num; ++i) {
            struct image* added = add_source(sources[i]);
            if (!img && added) {
                img = added;
            }
        }
    }

    return img;
}

/**
 * Construct image list by loading text lists.
 * @param files array of list files
 * @param num number of sources in the array
 */
static void load_fromfile(const char* const* files, size_t num)
{
    ctx.all_files = false; // not applicable in this mode

    for (size_t i = 0; i < num; ++i) {
        char* line = NULL;
        size_t line_sz = 0;
        ssize_t rd;
        FILE* fd;

        fd = fopen(files[i], "r");
        if (!fd) {
            const int rc = errno;
            fprintf(stderr, "Unable to open list file %s: [%i] %s\n", files[i],
                    rc, strerror(rc));
            continue;
        }

        while ((rd = getline(&line, &line_sz, fd)) > 0) {
            while (rd && (line[rd - 1] == '\r' || line[rd - 1] == '\n')) {
                line[--rd] = 0;
            }
            if (*line) {
                add_source(line);
            }
        }

        free(line);
        fclose(fd);
    }
}

/** Reindex the image list. */
static void reindex(void)
{
    ctx.size = 0;
    list_for_each(ctx.images, struct image, it) {
        it->index = ++ctx.size;
    }
}

/** File system event handler. */
static void on_fsevent(enum fsevent type, const char* path)
{
    const size_t path_len = strlen(path);
    const bool is_dir = (path[path_len - 1] == '/'); // ends with '/'

    imglist_lock();

    switch (type) {
        case fsevent_create:
            if (is_dir) {
                if (ctx.recursive) {
                    struct image* img = add_dir(path, true);
                    if (img) {
                        app_on_imglist(img, type);
                    }
                }
            } else {
                struct stat st;
                if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
                    struct image* img = add_entry(path, &st, true);
                    if (img) {
                        app_on_imglist(img, type);
                    }
                }
            }
            break;
        case fsevent_remove:
            if (!is_dir) {
                struct image* img = imglist_find(path);
                if (img) {
                    app_on_imglist(img, type);
                    imglist_remove(img);
                }
            }
            break;
        case fsevent_modify:
            if (!is_dir) {
                struct image* img = imglist_find(path);
                if (img) {
                    app_on_imglist(img, type);
                }
            }
            break;
    }

    reindex();
    imglist_unlock();
}

/**
 * Get nearest image with different parent (dir).
 * @param img start entry
 * @param loop enable/disable loop mode
 * @param forward direction (forward/backward)
 * @return image instance or NULL if not found
 */
static struct image* get_diff_parent(struct image* img, bool loop, bool forward)
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
            it = imglist_next(it, loop);
        } else {
            it = imglist_prev(it, loop);
        }
        if (!it) {
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

void imglist_init(const struct config* cfg)
{
    const struct config* section = config_section(cfg, CFG_LIST);

    pthread_mutex_init(&ctx.lock, NULL);

    ctx.order = config_get_oneof(section, CFG_LIST_ORDER, order_names,
                                 ARRAY_SIZE(order_names));
    ctx.reverse = config_get_bool(section, CFG_LIST_REVERSE);
    ctx.recursive = config_get_bool(section, CFG_LIST_RECURSIVE);
    ctx.all_files = config_get_bool(section, CFG_LIST_ALL);
    ctx.from_file = config_get_bool(section, CFG_LIST_FROMFILE);

    if (config_get_bool(section, CFG_LIST_FSMON)) {
        fs_monitor_init(on_fsevent);
    }
}

void imglist_destroy(void)
{
    fs_monitor_destroy();

    list_for_each(ctx.images, struct image, it) {
        image_free(it, IMGDATA_SELF);
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

bool imglist_is_locked(void)
{
    if (pthread_mutex_trylock(&ctx.lock) == 0) {
        pthread_mutex_unlock(&ctx.lock);
        return false;
    }
    return true;
}

struct image* imglist_load(const char* const* sources, size_t num)
{
    struct image* img = NULL;

    assert(ctx.size == 0 && "already loaded");

    if (ctx.from_file) {
        load_fromfile(sources, num);
    } else {
        img = load_sources(sources, num);
    }

    if (ctx.size) {
        sort();
        reindex();
        if (ctx.from_file) {
            img = imglist_first();
        }
    }

    return img;
}

void imglist_remove(struct image* img)
{
    ctx.images = list_remove(img);
    image_free(img, IMGDATA_SELF);
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

struct image* imglist_next(struct image* img, bool loop)
{
    struct image* next = list_next(img);

    if (!next && loop) {
        next = ctx.images;
        if (next) {
            if (next == img) {
                next = NULL;
            }
        }
    }

    return next;
}

struct image* imglist_prev(struct image* img, bool loop)
{
    struct image* prev = list_prev(img);

    if (!prev && loop) {
        prev = list_get_last(ctx.images);
        if (prev) {
            if (prev == img) {
                prev = NULL;
            }
        }
    }

    return prev;
}

struct image* imglist_next_parent(struct image* img, bool loop)
{
    return get_diff_parent(img, loop, true);
}

struct image* imglist_prev_parent(struct image* img, bool loop)
{
    return get_diff_parent(img, loop, false);
}

struct image* imglist_rand(struct image* img)
{
    struct image* next = img;
    ssize_t offset;

    if (ctx.size == 1) {
        return NULL;
    }

    offset = rand() % (ctx.size + 1);
    while (--offset > 0 || next == img) {
        next = list_next(next);
        if (!next) {
            next = ctx.images;
        }
    }

    return next;
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
