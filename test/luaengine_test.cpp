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

TEST(LuaEngineTest, RemovedScrollButton)
{
    LuaEngine lua;
    lua.initialize(TEST_DATA_DIR "/../../extra/example.lua");

    testing::internal::CaptureStderr();
    lua.execute("swayimg.gallery.on_mouse('Ctrl-ScrollUp', function() end)");
    EXPECT_NE(testing::internal::GetCapturedStderr().find(
                  "ScrollUp was removed, use swayimg.gallery.on_scroll()"),
              std::string::npos);
}
