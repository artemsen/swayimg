// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "luaengine.hpp"

#include <gtest/gtest.h>

TEST(LuaEngineTest, Load)
{
    testing::internal::CaptureStderr();

    LuaEngine lua;
    lua.initialize(TEST_DATA_DIR "/../../extra/example.lua");

    EXPECT_EQ(testing::internal::GetCapturedStderr(), "");
}
