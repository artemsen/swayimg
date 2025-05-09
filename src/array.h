// SPDX-License-Identifier: MIT
// Arrays and strings.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))
#endif

/** String slice. */
struct str_slice {
    const char* value;
    size_t len;
};

/** 1d array. */
struct array {
    size_t size;      ///< Total number of items in array
    size_t item_size; ///< Size of single item
    uint8_t data[1];  ///< Array data (variable length)
};

/**
 * Create array.
 * @param size initial number of items in array
 * @param item_size number of bytes per item
 * @return pointer to new array or NULL on errors
 */
struct array* arr_create(size_t size, size_t item_size);

/**
 * Free array.
 * @param arr array instance to free
 */
void arr_free(struct array* arr);

/**
 * Resize array.
 * @param arr destination array instance
 * @param size needed number of items in array
 * @return pointer to reallocated array or NULL on errors
 */
struct array* arr_resize(struct array* arr, size_t size);

/**
 * Append items to array.
 * @param arr destination array instance
 * @param items pointer to the first item
 * @param num number of items to append
 * @return pointer to reallocated array or NULL on errors
 */
struct array* arr_append(struct array* arr, const void* items, size_t num);

/**
 * Remove item from array.
 * @param arr destination array instance
 * @param index item index
 */
void arr_remove(struct array* arr, size_t index);

/**
 * Get item pointer from array.
 * @param arr array instance
 * @param index target index of item
 * @return pointer to item data or NULL if index is out of range
 */
void* arr_nth(struct array* arr, size_t index);

/**
 * Duplicate string.
 * @param src source string to duplicate
 * @param dst pointer to destination buffer
 * @return pointer to new string, caller must free it
 */
char* str_dup(const char* src, char** dst);

/**
 * Append string.
 * @param src source string to append
 * @param dst pointer to destination buffer
 * @return pointer to merged string, caller must free it
 */
char* str_append(const char* src, size_t len, char** dst);

/**
 * Convert text string to number.
 * @param text text to convert
 * @param len length of the source string (0=auto)
 * @param value output variable
 * @param base numeric base
 * @return false if text has invalid format
 */
bool str_to_num(const char* text, size_t len, ssize_t* value, int base);

/**
 * Convert ASCII string to wide char format.
 * @param src source string to encode
 * @param dst pointer to destination buffer
 * @return pointer to wide string, caller must free it
 */
wchar_t* str_to_wide(const char* src, wchar_t** dst);

/**
 * Split string ("abc,def" -> "abc", "def").
 * @param text source string to split
 * @param delimiter delimiter character
 * @param slices output array of slices
 * @param max_slices max number of slices (size of array)
 * @return real number of slices in source string
 */
size_t str_split(const char* text, char delimeter, struct str_slice* slices,
                 size_t max_slices);

/**
 * Search for value in string array.
 * @param array source array of strings
 * @param array_sz number of strings in array
 * @param value text to search
 * @param value_len length of the value (0=auto)
 * @return index of value in array or -1 if not found
 */
ssize_t str_search_index(const char** array, size_t array_sz, const char* value,
                         size_t value_len);
#define str_index(a, v, s) str_search_index((a), ARRAY_SIZE(a), v, s)
