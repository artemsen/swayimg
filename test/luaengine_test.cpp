// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "luabinding.hpp"

#include <gtest/gtest.h>

namespace {

class LuaTest : public ::testing::Test {
protected:
    LuaTest()
        : lua_bind_root(lua_engine)
        , lua_bind_imglist(lua_engine, &ImageList::self())
        , lua_bind_text(lua_engine, &Text::self())
        , lua_bind_viewer(lua_engine, &Viewer::self())
        , lua_bind_slideshow(lua_engine, &Slideshow::self())
        , lua_bind_gallery(lua_engine, &Gallery::self())
    {
    }

    void SetUp() override
    {
        lua_bind_root.init(nullptr);
        lua_bind_imglist.init(LuaImageListApi::NS);
        lua_bind_text.init(LuaTextApi::NS);
        lua_bind_viewer.init(LuaViewerApi::NS);
        lua_bind_slideshow.init(LuaSlideshowApi::NS);
        lua_bind_gallery.init(LuaGalleryApi::NS);
    }

protected:
    LuaEngine lua_engine;               ///< Lua engine instance
    LuaRootApi lua_bind_root;           ///< Lua bindings for root namespace
    LuaImageListApi lua_bind_imglist;   ///< Lua bindings for image list
    LuaTextApi lua_bind_text;           ///< Lua bindings for text overlay
    LuaViewerApi lua_bind_viewer;       ///< Lua bindings for viewer mode
    LuaSlideshowApi lua_bind_slideshow; ///< Lua bindings for slideshow mode
    LuaGalleryApi lua_bind_gallery;     ///< Lua bindings for gallery mode
};

} // anonymous namespace

TEST_F(LuaTest, Load)
{
    testing::internal::CaptureStderr();
    lua_engine.load(TEST_DATA_DIR "/../../extra/example.lua");
    EXPECT_EQ(testing::internal::GetCapturedStderr(), "");
}

TEST_F(LuaTest, ErrorTracebackFiltersCFrames)
{
    // calling a C-bound API that raises an error that propagates through Lua
    // must not leak native function frames into the traceback
    testing::internal::CaptureStderr();
    lua_engine.execute("local function boom()\n"
                       "    swayimg.appid = ''\n"
                       "end\n"
                       "local wrapped = function() boom() end\n"
                       "wrapped()\n");
    const std::string output = testing::internal::GetCapturedStderr();
    EXPECT_NE(output.find("Application Id can not be empty"),
              std::string::npos);
    EXPECT_NE(output.find("in function 'boom'"), std::string::npos);
    EXPECT_EQ(output.find("[C]:"), std::string::npos);
}
