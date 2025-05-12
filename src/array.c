// SPDX-License-Identifier: MIT
// Arrays and strings.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "array.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

struct array* arr_create(size_t size, size_t item_size)
{
    struct array* arr;
    arr = calloc(1, sizeof(*arr) + size * item_size - sizeof(arr->data));
    if (arr) {
        arr->size = size;
        arr->item_size = item_size;
    }
    return arr;
}

void arr_free(struct array* arr)
{
    free(arr);
}

struct array* arr_resize(struct array* arr, size_t size)
{
    const size_t len = sizeof(*arr) + size * arr->item_size - sizeof(arr->data);

    if (!len) {
        return arr; // one uint8 item
    }

    arr = realloc(arr, len);
    if (arr) {
        arr->size = size;
    }

    return arr;
}

struct array* arr_append(struct array* arr, const void* items, size_t num)
{
    const size_t old_size = arr->size;

    arr = arr_resize(arr, arr->size + num);
    if (arr) {
        uint8_t* ptr = arr->data + old_size * arr->item_size;
        const size_t sz = num * arr->item_size;
        if (items) {
            memcpy(ptr, items, sz);
        } else {
            memset(ptr, 0, sz);
        }
    }

    return arr;
}

void arr_remove(struct array* arr, size_t index)
{
    void* ptr_left = arr_nth(arr, index);
    void* ptr_right = arr_nth(arr, index + 1);

    if (ptr_left) {
        --arr->size;
        if (ptr_right) {
            const size_t len = (arr->size - index) * arr->item_size;
            memmove(ptr_left, ptr_right, len);
        }
    }
}

void* arr_nth(struct array* arr, size_t index)
{
    return index < arr->size ? arr->data + arr->item_size * index : NULL;
}

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
