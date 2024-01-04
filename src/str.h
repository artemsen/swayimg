// SPDX-License-Identifier: MIT
// String operations.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

/** String slice. */
struct str_slice {
    const char* value;
    size_t len;
};

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
 * Convert ansi string to wide char format.
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
#define str_index(a, v, s) str_search_index((a), sizeof(a) / sizeof(*(a)), v, s)
