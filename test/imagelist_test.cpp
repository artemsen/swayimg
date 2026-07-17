// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "imagelist.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <format>

// NOLINTNEXTLINE(bugprone-throwing-static-initialization)
static const std::filesystem::path IMGLIST_TEST_DIR =
    std::filesystem::path(TEST_DATA_DIR) / "imagelist";

// Check real and expected lists for equality
static ::testing::AssertionResult
CheckImageLists(const std::vector<ImageEntryPtr>& real,
                const std::vector<std::filesystem::path>& expect)
{
    if (real.size() != expect.size()) {
        return ::testing::AssertionFailure() << "Size of lists doesn't match";
    }

    for (size_t i = 0; i < real.size(); ++i) {
        if (real[i]->path != expect[i]) {
            return ::testing::AssertionFailure()
                << "Path " << i << " doesn't match: expected " << expect[i]
                << ", but got " << real[i]->path;
        }
    }

    return ::testing::AssertionSuccess();
}

// Print real and expected lists
static std::string to_string(const std::vector<ImageEntryPtr>& real,
                             const std::vector<std::filesystem::path>& expect)
{
    std::string dump = "# | real         | expected\n";
    dump += "--+--------------+--------------";
    for (size_t i = 0; i < std::max(real.size(), expect.size()); ++i) {
        const std::string rpath =
            i < real.size() ? real[i]->path.string() : "NULL";
        const std::string epath =
            i < expect.size() ? expect[i].string() : "NULL";
        dump += std::format("\n{} | {:12} | {}", i, rpath, epath);
    }
    return dump;
}

#define EXPECT_ILEQ(il, expect) \
    EXPECT_TRUE(CheckImageLists(il, expect)) << to_string(il, expect)

TEST(ImageListTest, Size)
{
    ImageList il;

    ASSERT_EQ(il.size(), 0UL);
    il.add({ "exec://1" });
    ASSERT_EQ(il.size(), 1UL);
    il.add({ "exec://2", "exec://3" });
    ASSERT_EQ(il.size(), 3UL);
}

TEST(ImageListTest, AddNone)
{
    ImageList il;
    EXPECT_TRUE(il.add(std::vector<std::filesystem::path>()).empty());
    EXPECT_ILEQ(il.get_all(), {});
}

TEST(ImageListTest, AddOne)
{
    ImageList il;
    const auto added = il.add({ "exec://1" });
    const std::vector<std::filesystem::path> expected = { "exec://1" };
    EXPECT_ILEQ(added, expected);
    EXPECT_ILEQ(il.get_all(), expected);
}

TEST(ImageListTest, AddDuplicates)
{
    ImageList il;

    const auto added0 = il.add({ "exec://1", "exec://2", "exec://2" });
    const std::vector<std::filesystem::path> expected0 = { "exec://1",
                                                           "exec://2" };
    EXPECT_ILEQ(added0, expected0);
    EXPECT_ILEQ(il.get_all(), expected0);

    const auto added1 = il.add({ "exec://1", "exec://3" });
    const std::vector<std::filesystem::path> expected1 = { "exec://3" };
    EXPECT_ILEQ(added1, expected1);

    const std::vector<std::filesystem::path> final = {
        "exec://1",
        "exec://2",
        "exec://3",
    };
    EXPECT_ILEQ(il.get_all(), final);
}

TEST(ImageListTest, AddUnexistent)
{
    ImageList il;
    EXPECT_TRUE(il.add({ "/not/exists" }).empty());
    EXPECT_ILEQ(il.get_all(), {});
}

TEST(ImageListTest, AdjacentOff)
{
    ImageList il;
    il.adjacent = false;
    il.recursive = false;
    il.add({ IMGLIST_TEST_DIR / "file_0" });
    EXPECT_TRUE(il.find(IMGLIST_TEST_DIR / "file_0"));
    EXPECT_ILEQ(il.get_all(), { IMGLIST_TEST_DIR / "file_0" });
}

TEST(ImageListTest, AdjacentOn)
{
    ImageList il;
    il.adjacent = true;
    il.recursive = false;
    const auto added = il.add({ IMGLIST_TEST_DIR / "file_1" });

    const std::vector<std::filesystem::path> expected_a = {
        IMGLIST_TEST_DIR / "file_1",
        IMGLIST_TEST_DIR / "file_0",
    };
    EXPECT_ILEQ(added, expected_a);

    const std::vector<std::filesystem::path> expected_f = {
        IMGLIST_TEST_DIR / "file_0",
        IMGLIST_TEST_DIR / "file_1",
    };
    EXPECT_ILEQ(il.get_all(), expected_f);
}

TEST(ImageListTest, RecursiveOff)
{
    ImageList il;
    il.adjacent = false;
    il.recursive = false;
    il.add({ IMGLIST_TEST_DIR });

    const std::vector<std::filesystem::path> expected = {
        IMGLIST_TEST_DIR / "file_0",
        IMGLIST_TEST_DIR / "file_1",
    };
    EXPECT_ILEQ(il.get_all(), expected);
}

TEST(ImageListTest, RecursiveOn)
{
    ImageList il;
    il.adjacent = false;
    il.recursive = true;
    il.add({ IMGLIST_TEST_DIR });

    const std::vector<std::filesystem::path> expected = {
        IMGLIST_TEST_DIR / "file_0",
        IMGLIST_TEST_DIR / "file_1",
        IMGLIST_TEST_DIR / "subdir" / "file_sub0",
        IMGLIST_TEST_DIR / "subdir" / "file_sub1",
    };
    EXPECT_ILEQ(il.get_all(), expected);
}

TEST(ImageListTest, SourceOrder)
{
    ImageList il;
    il.set_order(ImageList::Order::Numeric);
    const std::vector<std::filesystem::path> paths = {
        "exec://3",
        "exec://1",
        "exec://2",
    };
    const auto added = il.add(paths);
    EXPECT_ILEQ(added, paths);
}

TEST(ImageListTest, BigNumber)
{
    ImageList il;
    il.set_order(ImageList::Order::Numeric);

    const std::vector<std::filesystem::path> paths = {
        "exec://712174062970435688724dab6dc3ceb.jpeg",
        "exec://7152b8159cf6a64e8dda2ff3f8ed40f.jpeg",
    };

    il.add(paths);
    EXPECT_ILEQ(il.get_all(), paths);
}

TEST(ImageListTest, SortNone)
{
    ImageList il;
    il.set_order(ImageList::Order::None);

    const std::vector<std::filesystem::path> paths = {
        "exec://2",
        "exec://3",
        "exec://1",
    };
    il.add(paths);
    EXPECT_ILEQ(il.get_all(), paths);
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
    il.add({
        paths[2],
        paths[0],
        paths[5],
        paths[3],
        paths[4],
        paths[1],
    });
    EXPECT_ILEQ(il.get_all(), paths);
}

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
    il.add({
        paths[2],
        paths[0],
        paths[5],
        paths[3],
        paths[4],
        paths[1],
    });
    EXPECT_ILEQ(il.get_all(), paths);
}

#if defined(__linux__) && defined(__GLIBC__)
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
    il.add({
        paths[2],
        paths[0],
        paths[5],
        paths[3],
        paths[4],
        paths[1],
    });
    EXPECT_ILEQ(il.get_all(), paths);
}
#endif // __linux__ && __GLIBC__

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
    il.add({
        paths[2],
        paths[0],
        paths[5],
        paths[3],
        paths[4],
        paths[1],
    });
    EXPECT_ILEQ(il.get_all(), paths);
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
    il.add({
        paths[2],
        paths[0],
        paths[5],
        paths[3],
        paths[4],
        paths[1],
    });
    EXPECT_ILEQ(il.get_all(), paths);
}

TEST(ImageListTest, SortTime)
{
    ImageList il;
    il.set_order(ImageList::Order::None);
    il.set_reverse(false);

    const std::vector<std::filesystem::path> paths = {
        "exec://0", "exec://1", "exec://2", "exec://3", "exec://4",
    };
    il.add(paths);
    EXPECT_ILEQ(il.get_all(), paths);

    std::time_t mtime = 1000;
    for (auto& it : il.get_all()) {
        if (it->path != "exec://2") {
            it->mtime = mtime;
        }
        --mtime;
    }

    il.set_order(ImageList::Order::Mtime);
    const std::vector<std::filesystem::path> expect = {
        "exec://2", "exec://4", "exec://3", "exec://1", "exec://0",
    };
    EXPECT_ILEQ(il.get_all(), expect);
}

TEST(ImageListTest, SortRandom)
{
    ImageList il;
    il.set_order(ImageList::Order::Random);

    const std::vector<std::filesystem::path> paths = {
        "exec://0", "exec://1", "exec://2", "exec://3",
        "exec://4", "exec://5", "exec://6", "exec://7",
    };
    il.add(paths);

    ImageEntryPtr entry = il.get(nullptr, ImageList::Dir::First);
    bool ordered = true;
    for (const auto& it : paths) {
        ASSERT_TRUE(entry);
        ordered &= entry->path == it;
        entry = il.get(entry, ImageList::Dir::Next);
    }

    EXPECT_FALSE(ordered);
}

TEST(ImageListTest, GetFirst)
{
    ImageList il;
    il.set_order(ImageList::Order::None);

    EXPECT_FALSE(il.get(nullptr, ImageList::Dir::First));

    il.add({ "exec://first", "exec://middle", "exec://last" });

    const ImageEntryPtr first = il.get(nullptr, ImageList::Dir::First);
    ASSERT_TRUE(first);
    EXPECT_EQ(first->path, "exec://first");
}

TEST(ImageListTest, GetLast)
{
    ImageList il;
    il.set_order(ImageList::Order::None);

    EXPECT_FALSE(il.get(nullptr, ImageList::Dir::Last));

    il.add({ "exec://first", "exec://middle", "exec://last" });

    const ImageEntryPtr last = il.get(nullptr, ImageList::Dir::Last);
    ASSERT_TRUE(last);
    EXPECT_EQ(last->path, "exec://last");
}

TEST(ImageListTest, GetNext)
{
    ImageList il;
    il.set_order(ImageList::Order::None);
    il.add({ "exec://first", "exec://last" });

    const ImageEntryPtr first = il.get(nullptr, ImageList::Dir::First);
    ASSERT_TRUE(first);

    const ImageEntryPtr next = il.get(first, ImageList::Dir::Next);
    ASSERT_TRUE(next);
    EXPECT_EQ(next->path, "exec://last");

    EXPECT_FALSE(il.get(next, ImageList::Dir::Next));
}

TEST(ImageListTest, GetPrev)
{
    ImageList il;
    il.set_order(ImageList::Order::None);
    il.add({ "exec://first", "exec://last" });

    const ImageEntryPtr last = il.get(nullptr, ImageList::Dir::Last);
    ASSERT_TRUE(last);

    const ImageEntryPtr prev = il.get(last, ImageList::Dir::Prev);
    ASSERT_TRUE(prev);
    EXPECT_EQ(prev->path, "exec://first");

    EXPECT_FALSE(il.get(prev, ImageList::Dir::Prev));
}

TEST(ImageListTest, GetNextParent)
{
    ImageList il;
    il.set_order(ImageList::Order::None);
    il.add({
        "exec://a/0",
        "exec://a/1",
        "exec://b/0",
        "exec://c/0",
        "exec://c/1",
    });

    ImageEntryPtr entry = il.get(nullptr, ImageList::Dir::First);
    ASSERT_TRUE(entry);

    entry = il.get(entry, ImageList::Dir::NextParent);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://b/0");

    entry = il.get(entry, ImageList::Dir::NextParent);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://c/0");

    entry = il.get(entry, ImageList::Dir::NextParent);
    ASSERT_FALSE(entry);
}

TEST(ImageListTest, GetPrevParent)
{
    ImageList il;
    il.set_order(ImageList::Order::None);
    il.add({
        "exec://a/0",
        "exec://a/1",
        "exec://b/0",
        "exec://c/0",
        "exec://c/1",
    });

    ImageEntryPtr entry = il.get(nullptr, ImageList::Dir::Last);
    ASSERT_TRUE(entry);

    entry = il.get(entry, ImageList::Dir::PrevParent);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://b/0");

    entry = il.get(entry, ImageList::Dir::PrevParent);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://a/1");

    entry = il.get(entry, ImageList::Dir::PrevParent);
    ASSERT_FALSE(entry);
}

TEST(ImageListTest, GetRandom)
{
    ImageList il;
    il.set_order(ImageList::Order::None);
    il.add({ "exec://1", "exec://2", "exec://3" });

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
    il.set_order(ImageList::Order::None);
    il.add({ "exec://1", "exec://2", "exec://3", "exec://4" });

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
    il.set_order(ImageList::Order::None);
    il.add({ "exec://1", "exec://2", "exec://3", "exec://4" });

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
    il.set_order(ImageList::Order::None);
    il.add({ "exec://1", "exec://2", "exec://3" });

    const ImageEntryPtr entry = il.find("exec://2");
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->path, "exec://2");

    ASSERT_FALSE(il.find("exec://22"));
    ASSERT_FALSE(il.find(""));
}

TEST(ImageListTest, Clear)
{
    ImageList il;
    il.set_order(ImageList::Order::None);

    const std::vector<std::filesystem::path> paths = {
        "exec://1",
        "exec://2",
        "exec://3",
    };
    il.add(paths);

    EXPECT_ILEQ(il.clear(), paths);
    EXPECT_ILEQ(il.get_all(), {});
}

TEST(ImageListTest, RemoveOne)
{
    ImageList il;
    il.set_order(ImageList::Order::None);
    il.add({ "exec://1", "exec://2", "exec://3" });

    const ImageEntryPtr entry = il.find("exec://2");
    ASSERT_TRUE(entry);
    EXPECT_FALSE(entry->removed);

    il.remove(entry);
    EXPECT_TRUE(entry->removed);

    const std::vector<std::filesystem::path> expected = { "exec://1",
                                                          "exec://3" };
    EXPECT_ILEQ(il.get_all(), expected);
}

TEST(ImageListTest, RemoveMultiple)
{
    ImageList il;
    il.set_order(ImageList::Order::None);
    il.add({
        "exec://1",
        "exec://2",
        "exec://3",
        "exec://4",
        "exec://5",
    });

    EXPECT_TRUE(il.remove(std::vector<std::filesystem::path> {}).empty());
    EXPECT_TRUE(il.remove({ "not_exists" }).empty());

    const auto removed = il.remove({ "exec://2", "exec://5", "exec://999" });
    EXPECT_EQ(removed.size(), 2UL);
    for (const auto& it : removed) {
        EXPECT_TRUE(it->removed);
    }

    const std::vector<std::filesystem::path> expected = { "exec://1",
                                                          "exec://3",
                                                          "exec://4" };
    EXPECT_ILEQ(il.get_all(), expected);
}

TEST(ImageListTest, RemoveByParent)
{
    ImageList il;
    il.adjacent = false;
    il.recursive = true;
    il.add({ IMGLIST_TEST_DIR });
    il.remove({ IMGLIST_TEST_DIR / "subdir" });

    const std::vector<std::filesystem::path> expected = {
        IMGLIST_TEST_DIR / "file_0",
        IMGLIST_TEST_DIR / "file_1",
    };
    EXPECT_ILEQ(il.get_all(), expected);
}

TEST(ImageListTest, GetRemoved)
{
    ImageList il;
    il.set_order(ImageList::Order::None);
    il.add({
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
