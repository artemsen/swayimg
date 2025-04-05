// SPDX-License-Identifier: MIT
// Filesystem watcher (inotify).
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "fswatch.h"

#include "application.h"
#include "buildcfg.h"
#include "list.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef HAVE_INOTIFY
void fswatch_init(void) { }
void fswatch_destroy(void) { }
void fswatch_set_file(__attribute__((unused)) const char* path,
                      __attribute__((unused)) fswatch_callback handler)
{
}
void fswatch_add_dir(__attribute__((unused)) const char* path,
                     __attribute__((unused)) fswatch_callback handler)
{
}
void fswatch_rm_dir(__attribute__((unused)) const char* path) { }
#else

#include <sys/inotify.h>

/** List of watched directories. */
struct watch_dir {
    struct list list; ///< Links to prev/next entry
    int id;           ///< inotify Id
    char path[1];     ///< Path to the directory (variable lenght)
};

/** Context of the file system watcher. */
struct fswatch {
    int notify;               ///< inotify file descriptor
    int file_id;              ///< Watched file id (currenty viewed file)
    fswatch_callback file_cb; ///< Event handler for file watch
    struct watch_dir* dirs;   ///< Watched directroies
    fswatch_callback dir_cb;  ///< Event handler for dir watch
};

/** Global fswatch context instance. */
static struct fswatch ctx;

/**
 * Handle inotify event.
 * @param event inotify event
 */
static void handle_event(const struct inotify_event* in_event)
{
    struct fswatch_event fs_event = { fswatch_create, false, NULL, NULL };

    // fill event
    fs_event.name = in_event->name;
    list_for_each(ctx.dirs, struct watch_dir, it) {
        if (it->id == in_event->wd) {
            fs_event.dir = it->path;
            break;
        }
    }

    // set event type
    if (in_event->wd == ctx.file_id) {
        assert(ctx.file_cb);
        fs_event.type = fswatch_modify;
        ctx.file_cb(&fs_event);
    } else {
        if (!fs_event.dir) {
            assert(false && "dir not found");
            return;
        }

        fs_event.is_dir = (in_event->mask & IN_ISDIR);

        // one of the watched directories updated
        if (in_event->mask & (IN_CREATE | IN_MOVED_TO)) {
            fs_event.type = fswatch_create;
        } else if (in_event->mask &
                   (IN_DELETE | IN_MOVED_FROM | IN_DELETE_SELF |
                    IN_MOVE_SELF)) {
            fs_event.type = fswatch_remove;
        } else {
            fs_event.type = fswatch_modify;
        }
        assert(ctx.dir_cb);
        ctx.dir_cb(&fs_event);
    }
}

/** inotify handler. */
static void on_inotify(__attribute__((unused)) void* data)
{
    while (true) {
        uint8_t buffer[PATH_MAX];
        ssize_t pos = 0;
        const ssize_t len = read(ctx.notify, buffer, sizeof(buffer));

        if (len < 0) {
            if (errno == EINTR) {
                continue;
            }
            break; // something went wrong
        }

        while (pos + sizeof(struct inotify_event) <= (size_t)len) {
            const struct inotify_event* event =
                (struct inotify_event*)&buffer[pos];
            if (!(event->mask & IN_IGNORED)) {
                handle_event(event);
            }
            pos += sizeof(struct inotify_event) + event->len;
        }
    }
}

void fswatch_init(void)
{
    ctx.file_id = -1;
    ctx.notify = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (ctx.notify >= 0) {
        app_watch(ctx.notify, on_inotify, NULL);
    }
}

void fswatch_destroy(void)
{
    if (ctx.notify >= 0) {
        if (ctx.file_id != -1) {
            inotify_rm_watch(ctx.notify, ctx.file_id);
        }
        list_for_each(ctx.dirs, struct watch_dir, it) {
            inotify_rm_watch(ctx.notify, it->id);
            free(it);
        }
        ctx.dirs = NULL;
        close(ctx.notify);
    }
}

void fswatch_set_file(const char* path, fswatch_callback handler)
{
    if (ctx.notify == -1) {
        return; // not available
    }

    if (ctx.file_id != -1) {
        inotify_rm_watch(ctx.notify, ctx.file_id);
        ctx.file_id = -1;
    }

    if (path) {
        ctx.file_cb = handler;
        ctx.file_id = inotify_add_watch(
            ctx.notify, path, IN_MODIFY | IN_MOVE_SELF | IN_DELETE_SELF);
    }
}

void fswatch_add_dir(const char* path, fswatch_callback handler)
{
    struct watch_dir* entry;
    size_t len;
    int id;

    if (ctx.notify == -1) {
        return; // not available
    }

    id = inotify_add_watch(ctx.notify, path,
                           IN_CREATE | IN_DELETE | IN_MOVE | IN_DELETE_SELF |
                               IN_MOVE_SELF);
    if (id == -1) {
        return;
    }

    len = strlen(path);
    entry = malloc(sizeof(struct watch_dir) + len);
    if (!entry) {
        inotify_rm_watch(id, ctx.notify);
        return;
    }

    entry->id = id;
    memcpy(entry->path, path, len + 1 /*last null*/);

    ctx.dirs = list_add(ctx.dirs, entry);
    ctx.dir_cb = handler;
}

void fswatch_rm_dir(const char* path)
{
    list_for_each(ctx.dirs, struct watch_dir, it) {
        if (strcmp(path, it->path) == 0) {
            ctx.dirs = list_remove(it);
            free(it);
            return;
        }
    }
    assert(false && "dir not found");
}

#endif // HAVE_INOTIFY
