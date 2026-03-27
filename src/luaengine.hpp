// SPDX-License-Identifier: MIT
// Lua integration.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.hpp"
#include "log.hpp"
#include "text.hpp"

// clang-format off
// lua headers must be included in this order
#include <lua.hpp>
#include <luabridge/LuaBridge.h>
#include <luabridge/Vector.h>
#include <luabridge/UnorderedMap.h>
// clang-format on

#include <filesystem>
#include <format>
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

    /**
     * Execute Lua script.
     * @param script script to execute
     */
    void execute(const std::string& script);

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
    [[nodiscard]] luabridge::LuaRef
    entry_to_table(const ImageEntry& entry) const;

    /**
     * Add reference to Lua object.
     * @param obj Lua reference to increment
     * @return created reference to use in future
     */
    luabridge::LuaRef* add_ref(const luabridge::LuaRef* obj);

    /**
     * Print warning about using a deprecated function.
     * @param name name of deprecated function
     * @param replacement description of replacement
     */
    void warn_deprecated(const char* name, const char* replacement) const;

    /**
     * Print Lua error message with stack trace.
     * @param fmt format description
     * @param ... format arguments
     */
    template <typename... Args>
    void print_error(const std::format_string<Args...> fmt,
                     Args&&... args) const
    {
        const std::string message =
            std::vformat(fmt.get(), std::make_format_args(args...));
        Log::error("{}", message);
        Text::self().set_status(message);

        // print stack trace
        lua_Debug entry;
        int level = 0;
        while (lua_getstack(lua_state, level, &entry)) {
            if (lua_getinfo(lua_state, "Sl", &entry) &&
                entry.currentline >= 0) {
                Log::error("  {}:{}", entry.short_src, entry.currentline);
            }
            level++;
        }
    }

private:
    lua_State* lua_state = nullptr;       ///< Lua state
    std::vector<luabridge::LuaRef*> refs; ///< Own Lua references
};
