// SPDX-License-Identifier: MIT
// Double linked list.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "list.h"

struct list* list_add_head(struct list* head, struct list* entry)
{
    entry->next = head;
    entry->prev = NULL;
    if (head) {
        head->prev = entry;
    }
    return entry;
}

struct list* list_append_tail(struct list* head, struct list* entry)
{
    struct list* last = NULL;

    if (!head) {
        head = entry;
    } else {
        last = head;
        while (last && last->next) {
            last = last->next;
        }
        last->next = entry;
    }

    entry->prev = last;
    entry->next = NULL;

    return head;
}

void list_insert_entry(struct list* after, struct list* entry)
{
    entry->prev = after;
    entry->next = after->next;
    after->next->prev = entry;
    after->next = entry;
}

struct list* list_remove_entry(struct list* entry)
{
    struct list* prev = entry->prev;
    struct list* next = entry->next;
    struct list* head = prev ? prev : next;

    if (prev) {
        prev->next = next;
    }
    if (next) {
        next->prev = prev;
    }
    while (head && head->prev) {
        head = head->prev;
    }
    return head;
}

struct list* list_get_last_entry(struct list* head)
{
    list_for_each(head, struct list, it) {
        if (list_is_last(it)) {
            return it;
        }
    }
    return head;
}

size_t list_size(const struct list* head)
{
    size_t sz = 0;
    list_for_each(head, const struct list, it) {
        ++sz;
    }
    return sz;
}
