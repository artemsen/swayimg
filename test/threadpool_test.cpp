// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "threadpool.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <set>

TEST(ThreadPoolTest, Size)
{
    {
        const ThreadPool tp(1);
        EXPECT_EQ(tp.size(), 1UL);
    }
    {
        const ThreadPool tp(8);
        EXPECT_GE(tp.size(), 1UL);
        EXPECT_LE(tp.size(), 8UL);
    }
}

TEST(ThreadPoolTest, SingleTaskExecution)
{
    ThreadPool tp(1);

    std::atomic<bool> executed = false;
    const size_t tid = tp.add([&executed]() {
        executed = true;
    });
    tp.wait(tid);

    EXPECT_TRUE(executed);
}

TEST(ThreadPoolTest, TaskResult)
{
    ThreadPool tp(1);

    std::atomic<int> result = 0;
    const size_t tid = tp.add(
        [&result](const int a, const int b) {
            result = a + b;
        },
        3, 7);
    tp.wait(tid);

    EXPECT_EQ(result, 10);
}

TEST(ThreadPoolTest, MultipleTasks)
{
    ThreadPool tp(2);

    std::atomic<size_t> counter = 0;
    constexpr size_t num_tasks = 100;

    std::vector<size_t> tids;
    tids.reserve(num_tasks);
    for (size_t i = 0; i < num_tasks; ++i) {
        tids.push_back(tp.add([&counter]() {
            ++counter;
        }));
    }

    tp.wait(tids);
    EXPECT_EQ(counter, num_tasks);
}

TEST(ThreadPoolTest, WaitAll)
{
    ThreadPool tp(2);

    std::atomic<size_t> counter = 0;
    constexpr size_t num_tasks = 50;

    for (size_t i = 0; i < num_tasks; ++i) {
        tp.add([&counter]() {
            ++counter;
        });
    }

    tp.wait();
    EXPECT_EQ(counter, num_tasks);
}

TEST(ThreadPoolTest, TaskIdsUnique)
{
    ThreadPool tp(1);

    std::set<size_t> ids;
    const size_t t1 = tp.add([]() {});
    const size_t t2 = tp.add([]() {});
    const size_t t3 = tp.add([]() {});

    ids.insert(t1);
    ids.insert(t2);
    ids.insert(t3);

    EXPECT_EQ(ids.size(), 3UL);
    EXPECT_NE(t1, t2);
    EXPECT_NE(t2, t3);
    EXPECT_NE(t1, t3);

    tp.wait();
}

TEST(ThreadPoolTest, Cancel)
{
    ThreadPool tp(1);

    std::atomic<bool> executed = false;
    tp.add([&executed]() {
        executed = true;
    });
    tp.wait();
    EXPECT_TRUE(executed);

    executed = false;

    // block the worker with a long task so queued tasks stay in queue
    std::mutex block_mutex;
    block_mutex.lock();
    tp.add([&block_mutex]() {
        block_mutex.lock();
        block_mutex.unlock();
    });

    tp.add([&executed]() {
        executed = true;
    });
    tp.cancel();

    // release the blocking task
    block_mutex.unlock();
    tp.wait();

    // the second task was canceled before execution
    EXPECT_FALSE(executed);
}

TEST(ThreadPoolTest, CancelThenAddNew)
{
    ThreadPool tp(1);

    tp.add([]() {});
    tp.wait();
    tp.cancel();

    std::atomic<bool> executed = false;
    const size_t tid = tp.add([&executed]() {
        executed = true;
    });
    tp.wait(tid);
    EXPECT_TRUE(executed);
}

TEST(ThreadPoolTest, WaitEmptyPool)
{
    ThreadPool tp(1);
    tp.wait();
    tp.wait();
}

TEST(ThreadPoolTest, WaitSpecificTaskWithQueuedTasks)
{
    ThreadPool tp(2);

    std::atomic<size_t> first = 0;
    std::atomic<size_t> second = 0;
    std::atomic<bool> first_started = false;

    const size_t slow_id = tp.add([&first, &first_started]() {
        first_started = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        first = 42;
    });

    const size_t fast_id = tp.add([&second]() {
        second = 99;
    });

    tp.wait(fast_id);
    EXPECT_EQ(second, 99UL);

    tp.wait(slow_id);
    EXPECT_EQ(first, 42UL);
}
