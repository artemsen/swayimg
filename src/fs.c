// SPDX-License-Identifier: MIT
// File system operations.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "fs.h"

#include "application.h"
#include "array.h"
#include "buildcfg.h"
#include "list.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef HAVE_INOTIFY
void fs_init(__attribute__((unused)) fs_callback handler) { }
void fs_destroy(void) { }
void fs_watch(__attribute__((unused)) const char* path) { }
#else

#include <sys/inotify.h>

/** List of watched files/directories. */
struct watch {
    struct list list; ///< Links to prev/next entry
    int id;           ///< inotify Id
    char path[1];     ///< Abolute path (variable length)
};

/** Context of the file system monitor. */
struct fs_monitor {
    int notify;            ///< inotify file descriptor
    struct watch* watch;   ///< Watched files/directories
    fs_monitor_cb handler; ///< Event handler
};

/** Global fs monitor context instance. */
static struct fs_monitor ctx = { -1, NULL, NULL };

/**
 * Handle inotify event.
 * @param event inotify event
 */
static void handle_event(const struct inotify_event* event)
{
    enum fsevent et;
    char path[PATH_MAX];

    if (event->mask & IN_IGNORED) {
        // remove from the watch list
        list_for_each(ctx.watch, struct watch, it) {
            if (it->id == event->wd) {
                ctx.watch = list_remove(it);
                free(it);
                break;
            }
        }
        return;
    }

    // get parent path
    *path = 0;
    list_for_each(ctx.watch, struct watch, it) {
        if (it->id == event->wd) {
            strncpy(path, it->path, sizeof(path));
            break;
        }
    }
    if (!*path) {
        assert(false && "no watch");
        return;
    }

    // compose full path
    if (event->len) {
        if (!fs_append_path(event->name, path, sizeof(path))) {
            return; // buffer too small
        }
        if (event->mask & IN_ISDIR) {
            fs_append_path(NULL, path, sizeof(path)); // add last slash
        }
    }

    // reduce event type
    if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
        et = fsevent_create;
    } else if (event->mask &
               (IN_DELETE | IN_MOVED_FROM | IN_DELETE_SELF | IN_MOVE_SELF)) {
        et = fsevent_remove;
    } else if (event->mask & IN_MODIFY) {
        et = fsevent_modify;
    } else {
        assert(false && "unhandled event");
        return;
    }

    ctx.handler(et, path);
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
            handle_event(event);
            pos += sizeof(struct inotify_event) + event->len;
        }
    }
}

void fs_monitor_init(fs_monitor_cb handler)
{
    ctx.notify = inotify_init1(IN_NONBLOCK);
    if (ctx.notify != -1) {
        ctx.handler = handler;
        app_watch(ctx.notify, on_inotify, NULL);
    }
}

void fs_monitor_destroy(void)
{
    if (ctx.notify != -1) {
        list_for_each(ctx.watch, struct watch, it) {
            inotify_rm_watch(ctx.notify, it->id);
            free(it);
        }
        ctx.watch = NULL;
        close(ctx.notify);
        ctx.notify = -1;
    }
}

void fs_monitor_add(const char* path)
{
    struct watch* entry;
    size_t len;
    int id;

    if (ctx.notify == -1) {
        return; // not available
    }

    // register inotify
    id = inotify_add_watch(ctx.notify, path,
                           IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVE |
                               IN_DELETE_SELF | IN_MOVE_SELF);
    if (id == -1) {
        return;
    }

    // allocate entry
    len = strlen(path);
    entry = malloc(sizeof(struct watch) + len);
    if (!entry) {
        inotify_rm_watch(id, ctx.notify);
        return;
    }
    entry->id = id;
    memcpy(entry->path, path, len + 1 /*last null*/);

    ctx.watch = list_add(ctx.watch, entry);
}

#endif // HAVE_INOTIFY

size_t fs_append_path(const char* file, char* path, size_t path_max)
{
    size_t file_len = file ? strlen(file) : 0;
    size_t path_len = strlen(path);

    if (path_len + file_len + 2 /* slash + last null */ >= path_max) {
        return 0;
    }

    if (path[path_len - 1] != '/') {
        path[path_len] = '/';
        path[++path_len] = 0;
    }

    if (file_len) {
        while (*file == '/') {
            ++file;
            --file_len;
        };
        memcpy(&path[path_len], file, file_len + 1);
        path_len += file_len;
    }

    return path_len;
}

size_t fs_abspath(const char* relative, char* path, size_t path_max)
{
    char buffer[PATH_MAX];
    struct str_slice dirs[1024];
    size_t dirs_num;
    size_t pos;

    if (*relative == '/') {
        strncpy(buffer, relative, sizeof(buffer) - 1);
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
        strncpy(buffer + len, relative, sizeof(buffer) - len - 1);
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

size_t fs_envpath(const char* env_name, const char* postfix, char* path,
                  size_t path_max)
{
    size_t postfix_len;
    size_t len = 0;

    if (env_name) {
        // add prefix from env var
        const char* delim;
        const char* env_val = getenv(env_name);
        if (!env_val || !*env_val) {
            return 0;
        }
        // use only the first directory if prefix is a list
        delim = strchr(env_val, ':');
        len = delim ? (size_t)(delim - env_val) : strlen(env_val);
        if (len + 1 >= path_max) {
            return 0;
        }
        memcpy(path, env_val, len + 1 /* last null */);
    }

    // append postfix
    postfix_len = strlen(postfix);
    if (len + postfix_len >= path_max) {
        return 0;
    }
    memcpy(path + len, postfix, postfix_len + 1 /* last null */);
    len += postfix_len;

    return len;
}
