// SPDX-License-Identifier: MIT
// Lua integration.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "fdevent.hpp"
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
#include <string>
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

    /**
     * Call a Lua function with debug.traceback as error handler.
     * @param ref reference to the Lua function to call
     * @param args arguments to pass
     */
    template <typename... Args>
    void call(const luabridge::LuaRef& ref, Args&&... args)
    {
        const auto rc = ref.callWithHandler(LuaEngine::traceback_lua,
                                            std::forward<Args>(args)...);
        if (!rc) {
            if (last_traceback.empty()) {
                last_traceback = "Lua call failed";
            }
            print_error("{}", last_traceback);
        }
    }

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
     * @return created reference to use it in future
     */
    const luabridge::LuaRef* add_ref(const luabridge::LuaRef* obj);

    /**
     * Print Lua error message.
     * @param fmt error text format
     * @param ... format arguments
     */
    template <typename... Args>
    void print_error(const std::format_string<Args...> fmt, Args&&... args)
    {
        const std::string message =
            std::vformat(fmt.get(), std::make_format_args(args...));
        const std::string status =
            message.substr(0, message.find_first_of('\n'));
        Log::error("{}", message);
        Text::self().set_status(status);
    }

    /**
     * Raise Lua error.
     * @param fmt error text format
     * @param ... format arguments
     */
    template <typename... Args>
    [[noreturn]]
    void raise_error(const std::format_string<Args...> fmt,
                     Args&&... args) const
    {
        const std::string message =
            std::vformat(fmt.get(), std::make_format_args(args...));
        luabridge::raise_lua_error(lua_state, "%s", message.c_str());
    }

    /**
      Handler used to get traceback of Lua call.
      Called by Lua BEFORE the call stack unwinds.
    */
    static int traceback_lua(lua_State* lua);

    /**
     * Print warning about using a deprecated function.
     * @param name name of deprecated function
     * @param replacement description of replacement
     */
    static void warn_deprecated(const char* name, const char* replacement);

private:
    lua_State* lua_state = nullptr;             ///< Lua state
    std::vector<const luabridge::LuaRef*> refs; ///< Own Lua references
    std::string last_traceback;

    FdTimer defer_timer;                   ///< Timer for deferred call
    luabridge::LuaRef* defer_fn = nullptr; ///< Deferred callback
};
