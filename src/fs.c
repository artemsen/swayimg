// SPDX-License-Identifier: MIT
// File system operations.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "fs.h"

#include "array.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
