// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "memdata.h"
}

#include <gtest/gtest.h>

#include <fstream>

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

TEST(List, Remove)
{
    struct list entry[3];
    struct list* head = NULL;

    memset(entry, 0xff, sizeof(entry)); // poison

    for (auto& it : entry) {
        head = list_add_head(head, &it);
    }

    head = list_remove_entry(&entry[1]);
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

TEST(Str, Duplicate)
{
    char* str = str_dup("Test123", NULL);
    ASSERT_NE(str, nullptr);
    EXPECT_STREQ(str, "Test123");
    EXPECT_NE(str_dup("NewTest123", &str), nullptr);
    EXPECT_NE(str, nullptr);
    EXPECT_STREQ(str, "NewTest123");
    free(str);
}

TEST(Str, Append)
{
    char* str = str_dup("Test", NULL);
    ASSERT_NE(str, nullptr);
    EXPECT_NE(str_append("123", 0, &str), nullptr);
    EXPECT_STREQ(str, "Test123");
    EXPECT_NE(str_append("ABCD", 2, &str), nullptr);
    EXPECT_STREQ(str, "Test123AB");
    free(str);
}

TEST(Str, ToNum)
{
    ssize_t num;

    ASSERT_TRUE(str_to_num("1234", 0, &num, 0));
    ASSERT_EQ(num, 1234);

    ASSERT_TRUE(str_to_num("1234", 2, &num, 0));
    ASSERT_EQ(num, 12);

    ASSERT_TRUE(str_to_num("0x1234", 0, &num, 0));
    ASSERT_EQ(num, 0x1234);

    ASSERT_TRUE(str_to_num("1234", 0, &num, 16));
    ASSERT_EQ(num, 0x1234);
}

TEST(Str, ToWide)
{
    wchar_t* str = str_to_wide("Test", NULL);
    ASSERT_NE(str, nullptr);
    EXPECT_STREQ(str, L"Test");
    EXPECT_NE(str_to_wide("NewTest123", &str), nullptr);
    EXPECT_NE(str, nullptr);
    EXPECT_STREQ(str, L"NewTest123");
    free(str);
}

TEST(Str, Split)
{
    struct str_slice slices[4];

    ASSERT_EQ(str_split("a,bc,def", ',', slices, ARRAY_SIZE(slices)),
              static_cast<size_t>(3));
    ASSERT_EQ(slices[0].len, static_cast<size_t>(1));
    ASSERT_EQ(strncmp(slices[0].value, "a", slices[0].len), 0);
    ASSERT_EQ(slices[1].len, static_cast<size_t>(2));
    ASSERT_EQ(strncmp(slices[1].value, "bc", slices[1].len), 0);
    ASSERT_EQ(slices[2].len, static_cast<size_t>(3));
    ASSERT_EQ(strncmp(slices[2].value, "def", slices[2].len), 0);

    ASSERT_EQ(str_split("", ';', slices, ARRAY_SIZE(slices)),
              static_cast<size_t>(0));
    ASSERT_EQ(str_split("a", ';', slices, ARRAY_SIZE(slices)),
              static_cast<size_t>(1));
    ASSERT_EQ(str_split("a;b;c;", ';', slices, ARRAY_SIZE(slices)),
              static_cast<size_t>(3));

    ASSERT_EQ(str_split("a,b,c,d,e,f", ',', slices, ARRAY_SIZE(slices)),
              static_cast<size_t>(6));
}

TEST(Str, SearchIndex)
{
    const char* array[] = { "param1", "param2", "param3" };
    ASSERT_EQ(str_index(array, "param2", 0), 1);
    ASSERT_EQ(str_index(array, "param22", 0), -1);
    ASSERT_EQ(str_index(array, "param22", 6), 1);
}
