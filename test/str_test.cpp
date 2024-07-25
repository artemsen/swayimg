// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "str.h"
}

#include <gtest/gtest.h>

#include <fstream>

TEST(Str, str_dup)
{
    char* str = str_dup("Test123", NULL);
    ASSERT_NE(str, nullptr);
    EXPECT_STREQ(str, "Test123");
    EXPECT_NE(str_dup("NewTest123", &str), nullptr);
    EXPECT_NE(str, nullptr);
    EXPECT_STREQ(str, "NewTest123");
    free(str);
}

TEST(Str, str_append)
{
    char* str = str_dup("Test", NULL);
    ASSERT_NE(str, nullptr);
    EXPECT_NE(str_append("123", 0, &str), nullptr);
    EXPECT_STREQ(str, "Test123");
    EXPECT_NE(str_append("ABCD", 2, &str), nullptr);
    EXPECT_STREQ(str, "Test123AB");
    free(str);
}

TEST(Str, str_to_num)
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

TEST(Str, str_to_wide)
{
    wchar_t* str = str_to_wide("Test", NULL);
    ASSERT_NE(str, nullptr);
    EXPECT_STREQ(str, L"Test");
    EXPECT_NE(str_to_wide("NewTest123", &str), nullptr);
    EXPECT_NE(str, nullptr);
    EXPECT_STREQ(str, L"NewTest123");
    free(str);
}

TEST(Str, str_split)
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

TEST(Str, str_index)
{
    const char* array[] = { "param1", "param2", "param3" };
    ASSERT_EQ(str_index(array, "param2", 0), 1);
    ASSERT_EQ(str_index(array, "param22", 0), -1);
    ASSERT_EQ(str_index(array, "param22", 6), 1);
}
