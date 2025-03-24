// SPDX-License-Identifier: MIT
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "list.h"
}

#include <gtest/gtest.h>

TEST(List, Add)
{
    struct list entry[2];
    struct list* head = NULL;

    memset(entry, 0xff, sizeof(entry)); // poison

    head = list_add_head(head, &entry[0]);
    ASSERT_EQ(head, &entry[0]);
    ASSERT_EQ(head->next, nullptr);
    ASSERT_EQ(head->prev, nullptr);
    ASSERT_TRUE(list_is_last(head));
    ASSERT_EQ(list_size(head), static_cast<size_t>(1));

    head = list_add_head(head, &entry[1]);
    ASSERT_EQ(head, &entry[1]);
    ASSERT_EQ(head->next, &entry[0]);
    ASSERT_EQ(head->prev, nullptr);
    ASSERT_FALSE(list_is_last(head));
    ASSERT_EQ(entry[0].next, nullptr);
    ASSERT_EQ(entry[0].prev, &entry[1]);
    ASSERT_EQ(list_size(head), static_cast<size_t>(2));
}

TEST(List, Append)
{
    struct list entry[2];
    struct list* head = NULL;

    memset(entry, 0xff, sizeof(entry)); // poison

    head = list_append_tail(head, &entry[0]);
    ASSERT_EQ(head, &entry[0]);
    ASSERT_EQ(head->next, nullptr);
    ASSERT_EQ(head->prev, nullptr);

    head = list_append_tail(head, &entry[1]);
    ASSERT_EQ(head, &entry[0]);
    ASSERT_EQ(head->next, &entry[1]);
    ASSERT_EQ(head->prev, nullptr);
    ASSERT_EQ(entry[1].next, nullptr);
    ASSERT_EQ(entry[1].prev, &entry[0]);
}

TEST(List, Insert)
{
    struct list entry[3];
    struct list insert_middle;
    struct list insert_start;
    struct list* head = NULL;

    memset(entry, 0xff, sizeof(entry)); // poison

    for (auto& it : entry) {
        head = list_append_tail(head, &it);
    }

    head = list_insert_entry(&entry[1], &insert_middle);
    ASSERT_EQ(head, &entry[0]);
    ASSERT_EQ(entry[0].next, &insert_middle);
    ASSERT_EQ(entry[1].prev, &insert_middle);
    ASSERT_EQ(insert_middle.prev, &entry[0]);
    ASSERT_EQ(insert_middle.next, &entry[1]);

    head = list_insert_entry(&entry[0], &insert_start);
    ASSERT_EQ(head, &insert_start);
    ASSERT_EQ(insert_start.prev, nullptr);
    ASSERT_EQ(insert_start.next, &entry[0]);
    ASSERT_EQ(entry[0].prev, &insert_start);
}

TEST(List, Remove)
{
    struct list entry[3];
    struct list* head = NULL;

    memset(entry, 0xff, sizeof(entry)); // poison

    for (auto& it : entry) {
        head = list_add_head(head, &it);
    }

    head = list_remove_entry(&entry[1]);
    ASSERT_EQ(entry[1].next, nullptr);
    ASSERT_EQ(entry[1].prev, nullptr);
    ASSERT_EQ(head, &entry[2]);
    ASSERT_EQ(head->next, &entry[0]);
    ASSERT_EQ(head->prev, nullptr);
    ASSERT_EQ(entry[0].next, nullptr);
    ASSERT_EQ(entry[0].prev, &entry[2]);

    head = list_remove_entry(&entry[0]);
    ASSERT_EQ(head, &entry[2]);
    ASSERT_EQ(head->next, nullptr);
    ASSERT_EQ(head->prev, nullptr);

    head = list_remove_entry(&entry[2]);
    ASSERT_EQ(head, nullptr);
}

TEST(List, ForEach)
{
    struct list entry[3];
    struct list* head = NULL;

    for (auto& it : entry) {
        head = list_add_head(head, &it);
    }

    size_t i = 0;
    list_for_each(head, struct list, it) {
        switch (i) {
            case 0:
                ASSERT_EQ(it, &entry[2]);
                break;
            case 1:
                ASSERT_EQ(it, &entry[1]);
                break;
            case 2:
                ASSERT_EQ(it, &entry[0]);
                break;
            default:
                GTEST_FAIL();
        }
        ++i;
    }
    ASSERT_EQ(list_size(head), static_cast<size_t>(3));
    ASSERT_EQ(i, static_cast<size_t>(3));
}

TEST(List, ForEachBack)
{
    struct list entry[3];
    struct list* head = NULL;

    for (auto& it : entry) {
        head = list_append_tail(head, &it);
    }

    size_t i = 0;
    list_for_each_back(&entry[2], struct list, it) {
        switch (i) {
            case 0:
                ASSERT_EQ(it, &entry[2]);
                break;
            case 1:
                ASSERT_EQ(it, &entry[1]);
                break;
            case 2:
                ASSERT_EQ(it, &entry[0]);
                break;
            default:
                GTEST_FAIL();
        }
        ++i;
    }
    ASSERT_EQ(list_size(head), static_cast<size_t>(3));
    ASSERT_EQ(i, static_cast<size_t>(3));
}
