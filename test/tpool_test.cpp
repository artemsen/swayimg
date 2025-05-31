// SPDX-License-Identifier: MIT
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "tpool.h"
}

#include <gtest/gtest.h>

static size_t worker_value = 42;
static void worker(void* data)
{
    worker_value = data ? *reinterpret_cast<size_t*>(data) : 4242;
}

static size_t deleter_value = 24;
static void deleter(void* data)
{
    deleter_value = data ? *reinterpret_cast<size_t*>(data) : 2424;
}

TEST(ThreadPool, Test)
{
    tpool_init();

    EXPECT_GE(tpool_threads(), static_cast<size_t>(1));

    size_t data = 1234567890;
    tpool_add_task(&worker, &deleter, &data);

    tpool_wait();
    tpool_cancel();

    EXPECT_EQ(worker_value, data);
    EXPECT_EQ(deleter_value, data);

    tpool_destroy();
}
