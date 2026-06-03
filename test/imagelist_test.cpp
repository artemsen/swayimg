// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "imagelist.hpp"

#include <gtest/gtest.h>

#include <format>

::testing::AssertionResult
CheckImageList(ImageList& il, const std::vector<std::filesystem::path>& expect)
{
    ImageEntryPtr entry = il.get(nullptr, ImageList::Dir::First);
    for (const auto& it : expect) {
        if (!entry) {
            return ::testing::AssertionFailure()
                << "Image list too short: expected next " << it;
        }
        if (entry->path != it) {
            return ::testing::AssertionFailure()
                << std::format("Invalid entry: got {}, expected {}",
                               entry->path.string(), it.string());
        }
        entry = il.get(entry, ImageList::Dir::Next);
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
    ImageEntryPtr entry = il.get(nullptr, ImageList::Dir::First);
    while (entry) {
        res += " " + entry->path.string();
        entry = il.get(entry, ImageList::Dir::Next);
        res += '\n';
    }
    return res;
}

TEST(ImageListTest, LoadEmpty)
{
    ImageList il;
    ASSERT_TRUE(il.load(std::vector<std::filesystem::path>()).empty());
}

static void test_first_loaded(const ImageList::Order order)
{
    // The first argument should always be returned first to make swayimg open
    // it. This ensures order if input args is respected.

    ImageList il;
    il.set_order(order);
    const std::vector<std::filesystem::path>& sources = {
        "exec://3",
        "exec://2",
        "exec://1",
    };
    const std::list<ImageEntryPtr> added = il.load(sources);
    EXPECT_EQ(added.size(), sources.size());
    EXPECT_EQ(il.size(), sources.size());
    EXPECT_EQ(added.front()->path, sources.front());
}

TEST(ImageListTest, LoadAlpha)
{
    test_first_loaded(ImageList::Order::Alpha);
}

TEST(ImageListTest, LoadNumeric)
{
    test_first_loaded(ImageList::Order::Numeric);
}

TEST(ImageListTest, LoadMtime)
{
    test_first_loaded(ImageList::Order::Mtime);
}

TEST(ImageListTest, LoadNone)
{
    test_first_loaded(ImageList::Order::None);
}

TEST(ImageListTest, LoadRandom)
{
    test_first_loaded(ImageList::Order::Random);
}

TEST(ImageListTest, LoadNotExists)
{
    ImageList il;
    il.recursive = false;
    ASSERT_TRUE(il.load(std::vector<std::filesystem::path>(
                            { TEST_DATA_DIR "/not_exists" }))
                    .empty());
}

TEST(ImageListTest, LoadFile)
{
    ImageList il;

    ImageEntryPtr entry = il.load(TEST_DATA_DIR "/filelist.txt");
    ASSERT_TRUE(entry);
    ASSERT_EQ(il.size(), 3UL);

    entry = il.get(nullptr, ImageList::Dir::First);
    ASSERT_TRUE(entry);
    EXPECT_FALSE(entry->removed);
    EXPECT_EQ(entry->index, 0UL);
    EXPECT_EQ(entry->path, "exec://1");

    entry = il.get(entry, ImageList::Dir::Next);
    ASSERT_TRUE(entry);
    EXPECT_FALSE(entry->removed);
    EXPECT_EQ(entry->index, 1UL);
    EXPECT_EQ(entry->path, "exec://2 ");

    entry = il.get(entry, ImageList::Dir::Next);
    ASSERT_TRUE(entry);
    EXPECT_FALSE(entry->removed);
    EXPECT_EQ(entry->index, 2UL);
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
                        })
                    .size() == 2UL);

    ImageEntryPtr entry = il.get(nullptr, ImageList::Dir::First);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://1");

    entry = il.get(entry, ImageList::Dir::Next);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://2");
}

TEST(ImageListTest, OutOfRange)
{
    ImageList il;
    il.set_order(ImageList::Order::Numeric);

    const std::vector<std::filesystem::path> paths = {
        "exec://712174062970435688724dab6dc3ceb.jpeg",
        "exec://7152b8159cf6a64e8dda2ff3f8ed40f.jpeg",
    };

    const ImageEntryPtr entry = il.load(paths).front();
    ASSERT_TRUE(entry);
}

TEST(ImageListTest, Unordered)
{
    ImageList il;
    il.set_order(ImageList::Order::None);

    const std::vector<std::filesystem::path> paths = {
        "exec://2",
        "exec://3",
        "exec://1",
    };

    const ImageEntryPtr entry = il.load(paths).front();
    ASSERT_TRUE(entry);
    EXPECT_FALSE(entry->removed);
    EXPECT_EQ(entry->path, "exec://2");
    EXPECT_ILEQ(il, paths);
}

TEST(ImageListTest, SortAlpha)
{
    ImageList il;
    il.set_order(ImageList::Order::Alpha);
    il.set_reverse(false);

    const std::vector<std::filesystem::path> paths = {
        /* 0 */ "exec://a/0",
        /* 1 */ "exec://a/1",
        /* 2 */ "exec://a/2",
        /* 3 */ "exec://a/b/0",
        /* 4 */ "exec://a/b/c/0",
        /* 5 */ "exec://ab/0",
    };

    const std::list<ImageEntryPtr> added = il.load({
        paths[2],
        paths[0],
        paths[5],
        paths[3],
        paths[4],
        paths[1],
    });
    ASSERT_FALSE(added.empty());
    const ImageEntryPtr& entry = added.front();

    EXPECT_FALSE(entry->removed);
    EXPECT_NE(entry->index, 0UL);
    EXPECT_EQ(entry->path, paths[2]);
    EXPECT_ILEQ(il, paths);
}

#ifdef __linux__
TEST(ImageListTest, SortAlphaUnicode)
{
    ImageList il;
    il.set_order(ImageList::Order::Alpha);
    il.set_reverse(false);

    const std::vector<std::filesystem::path> paths = {
        /* 0 */ "exec://a",
        /* 1 */ "exec://b",
        /* 2 */ "exec://е",
        /* 3 */ "exec://ё",
        /* 4 */ "exec://ж",
        /* 5 */ "exec://я",
    };

    const std::list<ImageEntryPtr> added = il.load({
        paths[2],
        paths[0],
        paths[5],
        paths[3],
        paths[4],
        paths[1],
    });
    ASSERT_FALSE(added.empty());
    const ImageEntryPtr& entry = added.front();

    EXPECT_FALSE(entry->removed);
    EXPECT_NE(entry->index, 0UL);
    EXPECT_EQ(entry->path, paths[2]);
    EXPECT_ILEQ(il, paths);
}
#endif // __linux__

TEST(ImageListTest, SortAlphaReverse)
{
    ImageList il;
    il.set_order(ImageList::Order::Alpha);
    il.set_reverse(true);

    const std::vector<std::filesystem::path> paths = {
        /* 0 */ "exec://ab/0",
        /* 1 */ "exec://a/b/c/0",
        /* 2 */ "exec://a/b/0",
        /* 3 */ "exec://a/2",
        /* 4 */ "exec://a/1",
        /* 5 */ "exec://a/0",
    };

    const std::list<ImageEntryPtr> added = il.load({
        paths[2],
        paths[0],
        paths[5],
        paths[3],
        paths[4],
        paths[1],
    });
    ASSERT_FALSE(added.empty());
    const ImageEntryPtr& entry = added.front();

    EXPECT_FALSE(entry->removed);
    EXPECT_NE(entry->index, 0UL);
    EXPECT_EQ(entry->path, paths[2]);
    EXPECT_ILEQ(il, paths);
}

TEST(ImageListTest, SortNumeric)
{
    ImageList il;
    il.set_order(ImageList::Order::Numeric);
    il.set_reverse(false);

    const std::vector<std::filesystem::path> paths = {
        /* 0 */ "exec://a/2",
        /* 1 */ "exec://a/10",
        /* 2 */ "exec://a/3/a",
        /* 3 */ "exec://a/10/a",
        /* 4 */ "exec://a/10b2/a",
        /* 5 */ "exec://a/10b10/a",
    };

    const std::list<ImageEntryPtr> added = il.load({
        paths[2],
        paths[0],
        paths[5],
        paths[3],
        paths[4],
        paths[1],
    });
    ASSERT_FALSE(added.empty());
    const ImageEntryPtr& entry = added.front();

    EXPECT_FALSE(entry->removed);
    EXPECT_NE(entry->index, 0UL);
    EXPECT_EQ(entry->path, paths[2]);
    EXPECT_ILEQ(il, paths);
}

TEST(ImageListTest, SortNumericReverse)
{
    ImageList il;
    il.set_order(ImageList::Order::Numeric);
    il.set_reverse(true);

    const std::vector<std::filesystem::path> paths = {
        /* 0 */ "exec://a/10b10/a",
        /* 1 */ "exec://a/10b2/a",
        /* 2 */ "exec://a/10/a",
        /* 3 */ "exec://a/3/a",
        /* 4 */ "exec://a/10",
        /* 5 */ "exec://a/2",
    };

    const std::list<ImageEntryPtr> added = il.load({
        paths[2],
        paths[0],
        paths[5],
        paths[3],
        paths[4],
        paths[1],
    });
    ASSERT_FALSE(added.empty());
    const ImageEntryPtr& entry = added.front();

    EXPECT_FALSE(entry->removed);
    EXPECT_NE(entry->index, 0UL);
    EXPECT_EQ(entry->path, paths[2]);
    EXPECT_ILEQ(il, paths);
}

TEST(ImageListTest, SortRandom)
{
    ImageList il;
    il.set_order(ImageList::Order::Random);

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

    ImageEntryPtr entry = il.get(nullptr, ImageList::Dir::First);
    bool ordered = true;
    for (auto& it : paths) {
        ASSERT_TRUE(entry);
        ordered &= entry->path == it;
        entry = il.get(entry, ImageList::Dir::Next);
    }

    EXPECT_FALSE(ordered);
}

TEST(ImageListTest, GetFirstLast)
{
    ImageList il;

    ASSERT_EQ(il.size(), 0UL);
    ASSERT_FALSE(il.get(nullptr, ImageList::Dir::First));
    ASSERT_FALSE(il.get(nullptr, ImageList::Dir::Last));

    il.add("exec://first");
    il.add("exec://last");

    ASSERT_EQ(il.size(), 2UL);

    ImageEntryPtr entry = il.get(nullptr, ImageList::Dir::First);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://first");

    entry = il.get(nullptr, ImageList::Dir::Last);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://last");
}

TEST(ImageListTest, GetNextPrev)
{
    ImageList il;

    il.add("exec://first");
    il.add("exec://last");

    ImageEntryPtr entry = il.get(nullptr, ImageList::Dir::First);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://first");
    ASSERT_FALSE(il.get(entry, ImageList::Dir::Prev));

    entry = il.get(entry, ImageList::Dir::Next);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://last");
    ASSERT_FALSE(il.get(entry, ImageList::Dir::Next));

    entry = il.get(entry, ImageList::Dir::Prev);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://first");
}

TEST(ImageListTest, GetNextPrevParent)
{
    ImageList il;
    il.load({
        "exec://a/0",
        "exec://a/1",
        "exec://b/0",
        "exec://c/0",
        "exec://c/1",
    });

    ImageEntryPtr entry;

    entry = il.get(il.get(nullptr, ImageList::Dir::First),
                   ImageList::Dir::NextParent);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://b/0");

    entry = il.get(entry, ImageList::Dir::NextParent);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://c/0");

    entry = il.get(entry, ImageList::Dir::NextParent);
    ASSERT_FALSE(entry);

    entry = il.get(il.get(nullptr, ImageList::Dir::Last),
                   ImageList::Dir::PrevParent);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://b/0");

    entry = il.get(entry, ImageList::Dir::PrevParent);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://a/1");
}

TEST(ImageListTest, GetRandom)
{
    ImageList il;
    il.load({
        "exec://1",
        "exec://2",
        "exec://3",
    });

    ImageEntryPtr entry;

    entry =
        il.get(il.get(nullptr, ImageList::Dir::First), ImageList::Dir::Random);
    ASSERT_TRUE(entry);
    EXPECT_NE(entry, il.get(nullptr, ImageList::Dir::First));

    entry =
        il.get(il.get(nullptr, ImageList::Dir::Last), ImageList::Dir::Random);
    ASSERT_TRUE(entry);
    EXPECT_NE(entry, il.get(nullptr, ImageList::Dir::Last));
}

TEST(ImageListTest, Advance)
{
    ImageList il;
    il.load({
        "exec://1",
        "exec://2",
        "exec://3",
        "exec://4",
    });

    EXPECT_FALSE(il.get(il.get(nullptr, ImageList::Dir::First), 100));
    EXPECT_FALSE(il.get(il.get(nullptr, ImageList::Dir::First), -100));
    EXPECT_FALSE(il.get(il.get(nullptr, ImageList::Dir::Last), 100));
    EXPECT_FALSE(il.get(il.get(nullptr, ImageList::Dir::Last), -100));

    ImageEntryPtr entry;

    entry = il.get(il.get(nullptr, ImageList::Dir::First), 2);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://3");

    entry = il.get(entry, -2);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://1");
}

TEST(ImageListTest, Distance)
{
    ImageList il;
    il.load({
        "exec://1",
        "exec://2",
        "exec://3",
        "exec://4",
    });

    EXPECT_EQ(il.distance(il.get(nullptr, ImageList::Dir::First),
                          il.get(nullptr, ImageList::Dir::Last)),
              3);
    EXPECT_EQ(il.distance(il.get(nullptr, ImageList::Dir::Last),
                          il.get(nullptr, ImageList::Dir::First)),
              -3);

    const ImageEntryPtr entry =
        il.get(il.get(nullptr, ImageList::Dir::First), ImageList::Dir::Next);
    EXPECT_EQ(il.distance(entry, entry), 0);
    EXPECT_EQ(il.distance(entry, il.get(nullptr, ImageList::Dir::Last)), 2);
    EXPECT_EQ(il.distance(entry, il.get(nullptr, ImageList::Dir::First)), -1);
}

TEST(ImageListTest, Find)
{
    ImageList il;
    il.load({
        "exec://1",
        "exec://2",
        "exec://3",
    });

    const ImageEntryPtr entry = il.find("exec://2");
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://2");

    ASSERT_FALSE(il.find("exec://22"));
    ASSERT_FALSE(il.find(""));
}

TEST(ImageListTest, Remove)
{
    ImageList il;
    il.load({
        "exec://1",
        "exec://2",
        "exec://3",
    });

    const ImageEntryPtr entry = il.find("exec://2");
    ASSERT_TRUE(entry);
    EXPECT_FALSE(entry->removed);

    il.remove(entry);
    ASSERT_TRUE(entry->removed);
    ASSERT_EQ(il.size(), 2UL);
}

TEST(ImageListTest, MoveFromRemoved)
{
    ImageList il;
    il.set_order(ImageList::Order::None);
    il.load({
        "exec://0",
        "exec://1",
        "exec://2",
        "exec://3",
        "exec://4",
    });
    ImageEntryPtr removed = il.find("exec://2");
    ASSERT_TRUE(removed);
    il.remove(removed);

    ImageEntryPtr entry;

    entry = il.get(removed, ImageList::Dir::Next);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://3");

    entry = il.get(removed, ImageList::Dir::Prev);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://1");

    entry = il.get(removed, ImageList::Dir::NextParent);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://3");

    entry = il.get(removed, ImageList::Dir::PrevParent);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://1");

    entry = il.get(nullptr, ImageList::Dir::First);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://0");

    entry = il.get(nullptr, ImageList::Dir::Last);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://4");

    entry = il.get(removed, ImageList::Dir::Random);
    ASSERT_TRUE(entry);
    EXPECT_NE(entry->path, "exec://2");

    // remove first
    removed = il.find("exec://0");
    ASSERT_TRUE(removed);
    il.remove(removed);

    entry = il.get(removed, ImageList::Dir::Next);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://1");

    entry = il.get(removed, ImageList::Dir::Prev);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://1");

    // remove last
    removed = il.find("exec://4");
    ASSERT_TRUE(removed);
    il.remove(removed);

    entry = il.get(removed, ImageList::Dir::Next);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://3");

    entry = il.get(removed, ImageList::Dir::Prev);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://3");
}
