// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "fdevent.hpp"

#include <fcntl.h>
#include <gtest/gtest.h>

TEST(FdEventTest, CloseOnExec)
{
    // child processes (started from Lua) must not inherit our descriptors
    const FdEvent event;
    ASSERT_NE(event.fd, -1);
    EXPECT_TRUE(fcntl(event.fd, F_GETFD) & FD_CLOEXEC);

    const FdTimer timer;
    ASSERT_NE(timer.fd, -1);
    EXPECT_TRUE(fcntl(timer.fd, F_GETFD) & FD_CLOEXEC);
}
