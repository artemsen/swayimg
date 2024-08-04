// SPDX-License-Identifier: MIT
// String operations.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "str.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

char* str_dup(const char* src, char** dst)
{
    const size_t sz = strlen(src) + 1;
    char* buffer = realloc(dst ? *dst : NULL, sz);

    if (buffer) {
        memcpy(buffer, src, sz);
        if (dst) {
            *dst = buffer;
        }
    }

    return buffer;
}

char* str_append(const char* src, size_t len, char** dst)
{
    const size_t src_len = len ? len : strlen(src);
    const size_t dst_len = dst && *dst ? strlen(*dst) : 0;
    const size_t buf_len = dst_len + src_len + 1 /* last null */;
    char* buffer = realloc(dst ? *dst : NULL, buf_len);

    if (buffer) {
        memcpy(buffer + dst_len, src, src_len);
        buffer[buf_len - 1] = 0;
        if (dst) {
            *dst = buffer;
        }
    }

    return buffer;
}

bool str_to_num(const char* text, size_t len, ssize_t* value, int base)
{
    char* endptr;
    long long num;
    char buffer[32];
    const char* ptr;

    if (!text) {
        return false;
    }

    if (!*text) {
        return false;
    }

    if (len == 0) {
        ptr = text;
    } else {
        if (len >= sizeof(buffer)) {
            len = sizeof(buffer) - 1;
        }
        memcpy(buffer, text, len);
        buffer[len] = 0;
        ptr = buffer;
    }

    errno = 0;
    num = strtoll(ptr, &endptr, base);

    if (!*endptr && errno == 0) {
        *value = num;
        return true;
    }

    return false;
}

wchar_t* str_to_wide(const char* src, wchar_t** dst)
{
    wchar_t* buffer;
    size_t len;

    len = mbstowcs(NULL, src, 0);
    if (len == (size_t)-1) {
        return NULL;
    }

    ++len; // last null
    buffer = realloc(dst ? *dst : NULL, len * sizeof(wchar_t));
    if (!buffer) {
        return NULL;
    }

    mbstowcs(buffer, src, len);

    if (dst) {
        *dst = buffer;
    }

    return buffer;
}

size_t str_split(const char* text, char delimeter, struct str_slice* slices,
                 size_t max_slices)
{
    size_t slice_num = 0;

    while (*text) {
        struct str_slice slice;

        // skip spaces
        while (*text && isspace(*text)) {
            ++text;
        }
        if (!*text) {
            break;
        }

        // construct slice
        if (*text == delimeter) {
            // empty value
            slice.value = "";
            slice.len = 0;
        } else {
            slice.value = text;
            while (*text && *text != delimeter) {
                ++text;
            }
            slice.len = text - slice.value;
            // trim spaces
            while (slice.len && isspace(slice.value[slice.len - 1])) {
                --slice.len;
            }
        }

        // add to output array
        if (slices && slice_num < max_slices) {
            memcpy(&slices[slice_num], &slice, sizeof(slice));
        }
        ++slice_num;

        if (*text) {
            ++text; // skip delimiter
        }
    }

    return slice_num;
}

ssize_t str_search_index(const char** array, size_t array_sz, const char* value,
                         size_t value_len)
{
    if (value_len == 0) {
        value_len = strlen(value);
    }

    for (size_t i = 0; i < array_sz; ++i) {
        const char* check = array[i];
        if (strlen(check) == value_len &&
            strncmp(value, check, value_len) == 0) {
            return i;
        }
    }
    return -1;
}
