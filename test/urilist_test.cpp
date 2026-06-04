// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "urilist.hpp"

#include <gtest/gtest.h>

TEST(UriListTest, ParseEmpty)
{
    const std::vector<std::filesystem::path> paths = urilist_parse("");
    EXPECT_TRUE(paths.empty());
}

TEST(UriListTest, ParseGood)
{
    std::string ul;
    ul += "file:///a/1\r\n";
    ul += "# file:///comment\r\n";
    ul += "\r\n";
    ul += "file:///a/2";
    const std::vector<std::filesystem::path> paths = urilist_parse(ul);
    ASSERT_EQ(paths.size(), 2UL);
    EXPECT_EQ(paths[0], "/a/1");
    EXPECT_EQ(paths[1], "/a/2");
}

TEST(UriListTest, ParseBad)
{
    std::string ul;
    ul += "/a/1\r\n";
    ul += "file:///%0w\r\n";
    const std::vector<std::filesystem::path> paths = urilist_parse(ul);
    EXPECT_TRUE(paths.empty());
}

TEST(UriListTest, ParseSafe)
{
    std::string ul;
    ul += "file:///a/%201/%252\r\n";
    const std::vector<std::filesystem::path> paths = urilist_parse(ul);
    ASSERT_EQ(paths.size(), 1UL);
    EXPECT_EQ(paths[0], "/a/ 1/%2");
}

TEST(UriListTest, Create)
{
    EXPECT_EQ(urilist_create("/a/1"), "file:///a/1\r\n");
    EXPECT_EQ(urilist_create("/ /%"), "file:///%20/%25\r\n");
}
