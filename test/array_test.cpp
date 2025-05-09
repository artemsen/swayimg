// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "array.h"
}

#include <gtest/gtest.h>

#include <fstream>

class Array : public ::testing::Test {
protected:
    void TearDown() override { arr_free(arr); }
    struct array* arr = nullptr;
};

TEST_F(Array, Create)
{
    arr = arr_create(10, 20);
    ASSERT_TRUE(arr);
    EXPECT_EQ(arr->size, static_cast<size_t>(10));
    EXPECT_EQ(arr->item_size, static_cast<size_t>(20));
}

TEST_F(Array, Resize)
{
    arr = arr_create(10, sizeof(size_t));
    ASSERT_TRUE(arr);
    EXPECT_EQ(arr->size, static_cast<size_t>(10));

    arr = arr_resize(arr, 50);
    ASSERT_TRUE(arr);
    EXPECT_EQ(arr->size, static_cast<size_t>(50));

    arr = arr_resize(arr, 10);
    ASSERT_TRUE(arr);
    EXPECT_EQ(arr->size, static_cast<size_t>(10));

    arr = arr_resize(arr, 0);
    ASSERT_TRUE(arr);
    EXPECT_EQ(arr->size, static_cast<size_t>(0));
}

TEST_F(Array, AppendOne)
{
    const uint8_t item = 42;

    arr = arr_create(0, sizeof(item));
    ASSERT_TRUE(arr);
    EXPECT_EQ(arr->size, static_cast<size_t>(0));

    arr = arr_append(arr, &item, 1);
    ASSERT_TRUE(arr);
    EXPECT_EQ(arr->size, static_cast<size_t>(1));
    EXPECT_EQ(arr->data[0], item);

    arr = arr_append(arr, &item, 1);
    ASSERT_TRUE(arr);
    EXPECT_EQ(arr->size, static_cast<size_t>(2));
    EXPECT_EQ(arr->data[sizeof(item)], item);
}

TEST_F(Array, AppendMany)
{
    const size_t items[] = { 42, 43, 44 };

    arr = arr_create(0, sizeof(items[0]));
    ASSERT_TRUE(arr);
    EXPECT_EQ(arr->size, static_cast<size_t>(0));

    arr = arr_append(arr, items, ARRAY_SIZE(items));
    ASSERT_TRUE(arr);
    EXPECT_EQ(arr->size, static_cast<size_t>(ARRAY_SIZE(items)));
    EXPECT_EQ(*reinterpret_cast<const size_t*>(arr->data), items[0]);
}

TEST_F(Array, Remove)
{
    const size_t items[] = { 42, 43, 44 };

    arr = arr_create(3, sizeof(items[0]));
    ASSERT_TRUE(arr);
    EXPECT_EQ(arr->size, static_cast<size_t>(3));
    memcpy(arr->data, items, sizeof(items));

    arr_remove(arr, 999);

    arr_remove(arr, 1);
    EXPECT_EQ(arr->size, static_cast<size_t>(2));
    EXPECT_EQ(*reinterpret_cast<const size_t*>(arr_nth(arr, 0)), items[0]);
    EXPECT_EQ(*reinterpret_cast<const size_t*>(arr_nth(arr, 1)), items[2]);

    arr_remove(arr, 1);
    EXPECT_EQ(arr->size, static_cast<size_t>(1));
    EXPECT_EQ(*reinterpret_cast<const size_t*>(arr_nth(arr, 0)), items[0]);

    arr_remove(arr, 0);
    EXPECT_EQ(arr->size, static_cast<size_t>(0));
}

TEST_F(Array, Nth)
{
    arr = arr_create(0, sizeof(size_t));
    ASSERT_TRUE(arr);

    for (size_t i = 0; i < 10; ++i) {
        arr = arr_append(arr, &i, 1);
        ASSERT_TRUE(arr);
    }
    EXPECT_EQ(arr->size, static_cast<size_t>(10));

    for (size_t i = 0; i < 10; ++i) {
        const void* ptr = arr_nth(arr, i);
        const size_t val = *static_cast<const size_t*>(ptr);
        EXPECT_EQ(val, i);
    }

    EXPECT_FALSE(arr_nth(arr, 99999));
}

TEST(String, Duplicate)
{
    char* str = str_dup("Test123", NULL);
    ASSERT_NE(str, nullptr);
    EXPECT_STREQ(str, "Test123");
    EXPECT_NE(str_dup("NewTest123", &str), nullptr);
    EXPECT_NE(str, nullptr);
    EXPECT_STREQ(str, "NewTest123");
    free(str);
}

TEST(String, Append)
{
    char* str = str_dup("Test", NULL);
    ASSERT_NE(str, nullptr);
    EXPECT_NE(str_append("123", 0, &str), nullptr);
    EXPECT_STREQ(str, "Test123");
    EXPECT_NE(str_append("ABCD", 2, &str), nullptr);
    EXPECT_STREQ(str, "Test123AB");
    free(str);
}

TEST(String, ToNum)
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

TEST(String, ToWide)
{
    wchar_t* str = str_to_wide("Test", NULL);
    ASSERT_NE(str, nullptr);
    EXPECT_STREQ(str, L"Test");
    EXPECT_NE(str_to_wide("NewTest123", &str), nullptr);
    EXPECT_NE(str, nullptr);
    EXPECT_STREQ(str, L"NewTest123");
    free(str);
}

TEST(String, Split)
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

TEST(String, SearchIndex)
{
    const char* array[] = { "param1", "param2", "param3" };
    ASSERT_EQ(str_index(array, "param2", 0), 1);
    ASSERT_EQ(str_index(array, "param22", 0), -1);
    ASSERT_EQ(str_index(array, "param22", 6), 1);
}
