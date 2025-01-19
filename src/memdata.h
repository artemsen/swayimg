// SPDX-License-Identifier: MIT
// Manipulating data structures: arrays, strings and lists.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))
#endif

/** String slice. */
struct str_slice {
    const char* value;
    size_t len;
};

/** Double linked list. */
struct list {
    struct list* next;
    struct list* prev;
};

/**
 * Add new entry to the head.
 * @param head pointer to the list head
 * @param entry pointer to entry
 * @return new head pointer
 */
struct list* list_add_head(struct list* head, struct list* entry);
#define list_add(head, entry) \
    (void*)list_add_head((struct list*)head, (struct list*)entry)

/**
 * Append new entry to the tail.
 * @param head pointer to the list head
 * @param entry pointer to entry
 * @return new head pointer
 */
struct list* list_append_tail(struct list* head, struct list* entry);
#define list_append(head, entry) \
    (void*)list_append_tail((struct list*)head, (struct list*)entry)

/**
 * Remove entry from the list.
 * @param entry pointer to entry
 * @return new head pointer
 */
struct list* list_remove_entry(struct list* entry);
#define list_remove(entry) (void*)list_remove_entry((struct list*)entry)

/**
 * Check if entry is the last one.
 * @param entry pointer to entry
 * @return true if this is the last entry in list
 */
#define list_is_last(entry) (((struct list*)entry)->next == NULL)

/**
 * List iterator.
 * @param head pointer to the list head
 * @param type iterator type name
 * @param it iterator variable name
 */
#define list_for_each(head, type, it)                                      \
    for (type* it = head,                                                  \
               *it_next = head ? (type*)((struct list*)head)->next : NULL; \
         it;                                                               \
         it = it_next, it_next = it ? (type*)((struct list*)it)->next : NULL)

/**
 * Get number of entries in the list.
 * @param head pointer to the list head
 * @return length of the list
 */
size_t list_size(const struct list* head);

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
