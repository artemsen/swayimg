// SPDX-License-Identifier: MIT
// Lua integration.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.hpp"

// clang-format off
// lua headers must be included in this order
#include <lua.hpp>
#include <luabridge/LuaBridge.h>
#include <luabridge/Vector.h>
#include <luabridge/UnorderedMap.h>
// clang-format on

#include <filesystem>
#include <vector>

/** Lua integration. */
class LuaEngine {
public:
    /**
     * Get global instance of Lua engine.
     * @return Lua engine instance
     */
    static LuaEngine& self();

    ~LuaEngine();

    /**
     * Initialize Lua engine.
     * @param config path to the config file, can be empty to autodetect
     */
    void initialize(const std::filesystem::path& config);

private:
    // Bind API to Lua
    void bind_root_api();
    void bind_imagelist_api();
    void bind_text_api();
    void bind_viewer_api(const char* name);
    void bind_slideshow_api();
    void bind_gallery_api();
    void bind_appmode_api(const char* name);

    /**
     * Convert image entry to Lua table.
     * @param entry image entry to convert
     * @return Lua table object
     */
    luabridge::LuaRef entry_to_table(const ImageEntry& entry) const;

    /**
     * Add reference to Lua object.
     * @param obj Lua reference to increment
     * @return created reference to use in future
     */
    luabridge::LuaRef* add_ref(const luabridge::LuaRef* obj);

private:
    lua_State* lua_state = nullptr;       ///< Lua state
    std::vector<luabridge::LuaRef*> refs; ///< Own Lua references
};
