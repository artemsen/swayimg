// SPDX-License-Identifier: MIT
// Double linked list.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stddef.h>
#include <sys/types.h>

/** Double linked list. */
struct list {
    struct list* next;
    struct list* prev;
};

/** Move to the next entry. */
#define list_next(entry) (void*)((struct list*)entry)->next
/** Move to the previous entry. */
#define list_prev(entry) (void*)((struct list*)entry)->prev

/** Check if entry is the first one (head). */
#define list_is_first(entry) (((struct list*)entry)->prev == NULL)
/** Check if entry is the last one (tail). */
#define list_is_last(entry) (((struct list*)entry)->next == NULL)

/**
 * List forward iterator.
 * @param start pointer to the starting entry of the list
 * @param type iterator type name
 * @param it iterator variable name
 */
#define list_for_each(start, type, it)                                       \
    for (type* it = start,                                                   \
               *it_next = start ? (type*)((struct list*)start)->next : NULL; \
         it;                                                                 \
         it = it_next, it_next = it ? (type*)((struct list*)it)->next : NULL)

/**
 * List backward iterator.
 * @param start pointer to the starting entry of the list
 * @param type iterator type name
 * @param it iterator variable name
 */
#define list_for_each_back(start, type, it)                                  \
    for (type* it = start,                                                   \
               *it_prev = start ? (type*)((struct list*)start)->prev : NULL; \
         it;                                                                 \
         it = it_prev, it_prev = it ? (type*)((struct list*)it)->prev : NULL)

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
 * Insert new entry before the specified one.
 * @param before insert position
 * @param entry pointer to the entry to insert
 * @return new head pointer
 */
struct list* list_insert_entry(struct list* before, struct list* entry);
#define list_insert(before, entry) \
    (void*)list_insert_entry((struct list*)before, (struct list*)entry)

/**
 * Remove entry from the list.
 * @param entry pointer to entry
 * @return new head pointer
 */
struct list* list_remove_entry(struct list* entry);
#define list_remove(entry) (void*)list_remove_entry((struct list*)entry)

/**
 * Get the last entry from the list.
 * @param entry pointer to the head entry
 * @return last entry
 */
struct list* list_get_last_entry(struct list* head);
#define list_get_last(head) (void*)list_get_last_entry((struct list*)head)

/**
 * Get number of entries in the list.
 * @param head pointer to the list head
 * @return length of the list
 */
size_t list_size(const struct list* head);
