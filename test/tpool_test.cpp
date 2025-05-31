// SPDX-License-Identifier: MIT
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "tpool.h"
}

#include <gtest/gtest.h>

static size_t test_value = 42;
static void callback(void* data)
{
    test_value = data ? *reinterpret_cast<size_t*>(data) : 4242;
}

TEST(ThreadPool, Test)
{
    tpool_init();

    EXPECT_GE(tpool_threads(), static_cast<size_t>(1));

    size_t new_val = 1234567890;
    tpool_add_task(&callback, &new_val);

    tpool_wait();
    tpool_cancel();

    EXPECT_EQ(test_value, new_val);

    tpool_destroy();
}
