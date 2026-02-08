// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "imagelist.hpp"

#include <gtest/gtest.h>

#include <format>

::testing::AssertionResult
CheckImageList(ImageList& il, const std::vector<std::filesystem::path>& expect)
{
    ImageList::EntryPtr entry = il.get(nullptr, ImageList::Pos::First);
    for (auto& it : expect) {
        if (!entry) {
            return ::testing::AssertionFailure()
                << "Image list too short: expected next " << it;
        }
        if (entry->path != it) {
            return ::testing::AssertionFailure()
                << std::format("Invalid entry: got {}, expected {}",
                               entry->path.string(), it.string());
        }
        entry = il.get(entry, ImageList::Pos::Next);
    }
    if (entry) {
        return ::testing::AssertionFailure()
            << "Image list too big: unexpected " << entry->path.string();
    }

    return ::testing::AssertionSuccess();
}

#define EXPECT_ILEQ(il, ex) \
    EXPECT_TRUE(CheckImageList(il, ex)) << "ImageList:\n" << to_string(il)

static std::string to_string(ImageList& il)
{
    std::string res;
    ImageList::EntryPtr entry = il.get(nullptr, ImageList::Pos::First);
    while (entry) {
        res += " " + entry->path.string();
        entry = il.get(entry, ImageList::Pos::Next);
        res += '\n';
    }
    return res;
}

TEST(ImageListTest, LoadFile)
{
    ImageList il;

    ImageList::EntryPtr entry = il.load(TEST_DATA_DIR "/filelist.txt");
    ASSERT_TRUE(entry);
    ASSERT_EQ(il.size(), 3UL);

    entry = il.get(nullptr, ImageList::Pos::First);
    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->valid());
    EXPECT_NE(entry->index, 0UL);
    EXPECT_EQ(entry->path, "exec://1");

    entry = il.get(entry, ImageList::Pos::Next);
    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->valid());
    EXPECT_NE(entry->index, 0UL);
    EXPECT_EQ(entry->path, "exec://2 ");

    entry = il.get(entry, ImageList::Pos::Next);
    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->valid());
    EXPECT_NE(entry->index, 0UL);
    EXPECT_EQ(entry->path, "exec://3\t");
}

TEST(ImageListTest, AddDir)
{
    ImageList il;
    il.add(TEST_DATA_DIR);
    ASSERT_NE(il.size(), 0UL);
}

TEST(ImageListTest, Duplicates)
{
    ImageList il;
    ASSERT_TRUE(il.load({
        "exec://1",
        "exec://1",
        "exec://2",
        "exec://2",
    }));

    ASSERT_EQ(il.size(), 2UL);

    ImageList::EntryPtr entry = il.get(nullptr, ImageList::Pos::First);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://1");

    entry = il.get(entry, ImageList::Pos::Next);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://2");
}

TEST(ImageListTest, Unordered)
{
    ImageList il;
    il.order = ImageList::Order::None;

    const std::vector<std::filesystem::path> paths = {
        "exec://2",
        "exec://3",
        "exec://1",
    };

    ImageList::EntryPtr entry = il.load(paths);
    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->valid());
    EXPECT_NE(entry->index, 0UL);
    EXPECT_EQ(entry->path, "exec://2");
    EXPECT_ILEQ(il, paths);
}

TEST(ImageListTest, SortAlpha)
{
    ImageList il;
    il.order = ImageList::Order::Alpha;
    il.reverse = false;

    const std::vector<std::filesystem::path> paths = {
        /* 0 */ "exec://a/0",
        /* 1 */ "exec://a/1",
        /* 2 */ "exec://a/b0",
        /* 3 */ "exec://a/b/0",
        /* 4 */ "exec://a/b/c/0",
        /* 5 */ "exec://ab/0",
    };
    ImageList::EntryPtr entry = il.load({
        paths[2],
        paths[0],
        paths[5],
        paths[3],
        paths[4],
        paths[1],
    });

    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->valid());
    EXPECT_NE(entry->index, 0UL);
    EXPECT_EQ(entry->path, paths[2]);
    EXPECT_ILEQ(il, paths);
}

TEST(ImageListTest, SortAlphaReverse)
{
    ImageList il;
    il.order = ImageList::Order::Alpha;
    il.reverse = true;

    const std::vector<std::filesystem::path> paths = {
        /* 0 */ "exec://ab/0",
        /* 1 */ "exec://a/b/c/0",
        /* 2 */ "exec://a/b/0",
        /* 3 */ "exec://a/b0",
        /* 4 */ "exec://a/1",
        /* 5 */ "exec://a/0",
    };
    ImageList::EntryPtr entry = il.load({
        paths[2],
        paths[0],
        paths[5],
        paths[3],
        paths[4],
        paths[1],
    });

    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->valid());
    EXPECT_NE(entry->index, 0UL);
    EXPECT_EQ(entry->path, paths[2]);
    EXPECT_ILEQ(il, paths);
}

TEST(ImageListTest, SortNumeric)
{
    ImageList il;
    il.order = ImageList::Order::Numeric;
    il.reverse = false;

    const std::vector<std::filesystem::path> paths = {
        /* 0 */ "exec://a/2",
        /* 1 */ "exec://a/10",
        /* 2 */ "exec://a/3/a",
        /* 3 */ "exec://a/10/a",
        /* 4 */ "exec://a/10b2/a",
        /* 5 */ "exec://a/10b10/a",
    };

    ImageList::EntryPtr entry = il.load({
        paths[2],
        paths[0],
        paths[5],
        paths[3],
        paths[4],
        paths[1],
    });

    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->valid());
    EXPECT_NE(entry->index, 0UL);
    EXPECT_EQ(entry->path, paths[2]);
    EXPECT_ILEQ(il, paths);
}

TEST(ImageListTest, SortNumericReverse)
{
    ImageList il;
    il.order = ImageList::Order::Numeric;
    il.reverse = true;

    const std::vector<std::filesystem::path> paths = {
        /* 0 */ "exec://a/10b10/a",
        /* 1 */ "exec://a/10b2/a",
        /* 2 */ "exec://a/10/a",
        /* 3 */ "exec://a/3/a",
        /* 4 */ "exec://a/10",
        /* 5 */ "exec://a/2",
    };

    ImageList::EntryPtr entry = il.load({
        paths[2],
        paths[0],
        paths[5],
        paths[3],
        paths[4],
        paths[1],
    });

    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->valid());
    EXPECT_NE(entry->index, 0UL);
    EXPECT_EQ(entry->path, paths[2]);
    EXPECT_ILEQ(il, paths);
}

TEST(ImageListTest, SortRandom)
{
    ImageList il;
    il.order = ImageList::Order::Random;

    // clang-format off
    const char* paths[8] = {
        "exec://0",
        "exec://1",
        "exec://2",
        "exec://3",
        "exec://4",
        "exec://5",
        "exec://6",
        "exec://7",
    };
    // clang-format on

    for (auto& it : paths) {
        il.add(it);
    }

    ImageList::EntryPtr entry = il.get(nullptr, ImageList::Pos::First);
    bool ordered = true;
    for (auto& it : paths) {
        ASSERT_TRUE(entry);
        ordered &= entry->path == it;
        entry = il.get(entry, ImageList::Pos::Next);
    }

    EXPECT_FALSE(ordered);
}

TEST(ImageListTest, GetFirstLast)
{
    ImageList il;

    ASSERT_EQ(il.size(), 0UL);
    ASSERT_FALSE(il.get(nullptr, ImageList::Pos::First));
    ASSERT_FALSE(il.get(nullptr, ImageList::Pos::Last));

    il.add("exec://first");
    il.add("exec://last");

    ASSERT_EQ(il.size(), 2UL);

    ImageList::EntryPtr entry = il.get(nullptr, ImageList::Pos::First);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://first");

    entry = il.get(nullptr, ImageList::Pos::Last);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://last");
}

TEST(ImageListTest, GetNextPrev)
{
    ImageList il;

    il.add("exec://first");
    il.add("exec://last");

    ImageList::EntryPtr entry = il.get(nullptr, ImageList::Pos::First);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://first");
    ASSERT_FALSE(il.get(entry, ImageList::Pos::Prev));

    entry = il.get(entry, ImageList::Pos::Next);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://last");
    ASSERT_FALSE(il.get(entry, ImageList::Pos::Next));

    entry = il.get(entry, ImageList::Pos::Prev);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://first");
}

TEST(ImageListTest, GetNextPrevParent)
{
    ImageList il;
    ASSERT_TRUE(il.load({
        "exec://a/0",
        "exec://a/1",
        "exec://b/0",
        "exec://c/0",
        "exec://c/1",
    }));

    ImageList::EntryPtr entry;

    entry = il.get(il.get(nullptr, ImageList::Pos::First),
                   ImageList::Pos::NextParent);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://b/0");

    entry = il.get(entry, ImageList::Pos::NextParent);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://c/0");

    entry = il.get(entry, ImageList::Pos::NextParent);
    ASSERT_FALSE(entry);

    entry = il.get(il.get(nullptr, ImageList::Pos::Last),
                   ImageList::Pos::PrevParent);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://b/0");

    entry = il.get(entry, ImageList::Pos::PrevParent);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://a/1");
}

TEST(ImageListTest, GetRandom)
{
    ImageList il;
    ASSERT_TRUE(il.load({
        "exec://1",
        "exec://2",
        "exec://3",
    }));

    ImageList::EntryPtr entry;

    entry =
        il.get(il.get(nullptr, ImageList::Pos::First), ImageList::Pos::Random);
    ASSERT_TRUE(entry);
    EXPECT_NE(entry, il.get(nullptr, ImageList::Pos::First));

    entry =
        il.get(il.get(nullptr, ImageList::Pos::Last), ImageList::Pos::Random);
    ASSERT_TRUE(entry);
    EXPECT_NE(entry, il.get(nullptr, ImageList::Pos::Last));
}

TEST(ImageListTest, Advance)
{
    ImageList il;
    ASSERT_TRUE(il.load({
        "exec://1",
        "exec://2",
        "exec://3",
        "exec://4",
    }));

    EXPECT_FALSE(il.get(il.get(nullptr, ImageList::Pos::First), 100));
    EXPECT_FALSE(il.get(il.get(nullptr, ImageList::Pos::First), -100));
    EXPECT_FALSE(il.get(il.get(nullptr, ImageList::Pos::Last), 100));
    EXPECT_FALSE(il.get(il.get(nullptr, ImageList::Pos::Last), -100));

    ImageList::EntryPtr entry;

    entry = il.get(il.get(nullptr, ImageList::Pos::First), 2);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://3");

    entry = il.get(entry, -2);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://1");
}

TEST(ImageListTest, Distance)
{
    ImageList il;
    ASSERT_TRUE(il.load({
        "exec://1",
        "exec://2",
        "exec://3",
        "exec://4",
    }));

    EXPECT_EQ(il.distance(il.get(nullptr, ImageList::Pos::First),
                          il.get(nullptr, ImageList::Pos::Last)),
              3);
    EXPECT_EQ(il.distance(il.get(nullptr, ImageList::Pos::Last),
                          il.get(nullptr, ImageList::Pos::First)),
              -3);

    ImageList::EntryPtr entry =
        il.get(il.get(nullptr, ImageList::Pos::First), ImageList::Pos::Next);
    EXPECT_EQ(il.distance(entry, entry), 0);
    EXPECT_EQ(il.distance(entry, il.get(nullptr, ImageList::Pos::Last)), 2);
    EXPECT_EQ(il.distance(entry, il.get(nullptr, ImageList::Pos::First)), -1);
}

TEST(ImageListTest, Find)
{
    ImageList il;
    ASSERT_TRUE(il.load({
        "exec://1",
        "exec://2",
        "exec://3",
    }));

    ImageList::EntryPtr entry = il.find("exec://2");
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://2");

    ASSERT_FALSE(il.find("exec://22"));
    ASSERT_FALSE(il.find(""));
}

TEST(ImageListTest, Remove)
{
    ImageList il;
    ASSERT_TRUE(il.load({
        "exec://1",
        "exec://2",
        "exec://3",
    }));

    ASSERT_EQ(il.size(), 3UL);

    ImageList::EntryPtr entry = il.find("exec://2");
    ASSERT_TRUE(entry);
    ASSERT_TRUE(entry->valid());

    il.remove(entry);
    ASSERT_FALSE(entry->valid());
    ASSERT_EQ(il.size(), 2UL);
}
