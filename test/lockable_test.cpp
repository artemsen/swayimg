// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "sparam.hpp"

#include <gtest/gtest.h>

#include <string>

TEST(LockableTest, DefaultConstructed)
{
    Lockable<int> lock;
    lock.set(42);
    EXPECT_EQ(lock.get(), 42);
}

TEST(LockableTest, ImplicitConversion)
{
    Lockable<int> lock;
    lock.set(99);
    const int& val = lock;
    EXPECT_EQ(val, 99);
}

TEST(LockableTest, AssignUnlocked)
{
    Lockable<int> lock;
    lock = 42;
    EXPECT_EQ(lock.get(), 42);
    lock = 100;
    EXPECT_EQ(lock.get(), 100);
}

TEST(LockableTest, AssignLocked)
{
    Lockable<int> lock;
    lock.lock(42);
    EXPECT_EQ(lock.get(), 42);
    lock = 100;
    EXPECT_EQ(lock.get(), 42);
}

TEST(LockableTest, SetUnlocked)
{
    Lockable<int> lock;
    lock.set(42);
    EXPECT_EQ(lock.get(), 42);
    lock.set(100);
    EXPECT_EQ(lock.get(), 100);
}

TEST(LockableTest, SetLocked)
{
    Lockable<int> lock;
    lock.lock(42);
    lock.set(100);
    EXPECT_EQ(lock.get(), 42);
}

TEST(LockableTest, Lock)
{
    Lockable<int> lock;
    lock.lock(7);
    EXPECT_EQ(lock.get(), 7);
    lock.set(99);
    EXPECT_EQ(lock.get(), 7);
    lock = 88;
    EXPECT_EQ(lock.get(), 7);
}

TEST(LockableTest, Unlock)
{
    Lockable<int> lock;
    lock.lock(7);
    EXPECT_EQ(lock.get(), 7);
    int& val = lock.unlock();
    val = 99;
    EXPECT_EQ(lock.get(), 99);
    lock.set(88);
    EXPECT_EQ(lock.get(), 88);
}

TEST(LockableTest, StringType)
{
    Lockable<std::string> lock;
    EXPECT_TRUE(lock.get().empty());
    lock = "hello";
    EXPECT_EQ(lock.get(), "hello");
    lock.lock("world");
    lock = "ignored";
    EXPECT_EQ(lock.get(), "world");
}

TEST(LockableTest, StructType)
{
    struct Point {
        int x;
        int y;
    };
    Lockable<Point> lock;
    lock = Point(1, 2);
    EXPECT_EQ(lock.get().x, 1);
    EXPECT_EQ(lock.get().y, 2);
    lock.lock(Point(3, 4));
    lock.set(Point(5, 6));
    EXPECT_EQ(lock.get().x, 3);
    EXPECT_EQ(lock.get().y, 4);
}

TEST(LockableTest, BoolType)
{
    Lockable<bool> lock;
    lock.set(true);
    EXPECT_TRUE(lock.get());
    lock.lock(false);
    lock.set(true);
    EXPECT_FALSE(lock.get());
}

TEST(LockableTest, IndependentInstances)
{
    Lockable<int> a;
    Lockable<int> b;
    a.lock(10);
    b = 20;
    EXPECT_EQ(a.get(), 10);
    EXPECT_EQ(b.get(), 20);
    b.lock(30);
    EXPECT_EQ(b.get(), 30);
}

TEST(LockableTest, AppModeType)
{
    Lockable<AppMode::Type> lock;
    lock.set(AppMode::Type::Viewer);
    EXPECT_EQ(lock.get(), AppMode::Type::Viewer);
    lock.lock(AppMode::Slideshow);
    lock.set(AppMode::Gallery);
    EXPECT_EQ(lock.get(), AppMode::Slideshow);
}

TEST(LockableTest, SizeType)
{
    Lockable<Size> lock;
    lock = Size(100, 200);
    EXPECT_EQ(lock.get().width, 100UL);
    EXPECT_EQ(lock.get().height, 200UL);
    lock.lock(Size(50, 60));
    lock.set(Size(70, 80));
    EXPECT_EQ(lock.get().width, 50UL);
    EXPECT_EQ(lock.get().height, 60UL);
}

TEST(LockableTest, UnlockAndModify)
{
    Lockable<int> lock;
    lock.set(10);
    lock.lock(20);
    EXPECT_EQ(lock.get(), 20);
    lock = 30;
    EXPECT_EQ(lock.get(), 20);
    int& ref = lock.unlock();
    EXPECT_EQ(ref, 20);
    ref = 40;
    EXPECT_EQ(lock.get(), 40);
    lock.set(50);
    EXPECT_EQ(lock.get(), 50);
}
