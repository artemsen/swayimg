// SPDX-License-Identifier: MIT
// Lua integration.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "appmode.hpp"

#include <string>
#include <vector>

// forward declaration to avoid include lua headers
namespace luabridge {
class LuaRef;
}
struct lua_State;

/** Lua integration. */
class LuaEngine {
public:
    ~LuaEngine();

    /**
     * Initialize Lua engine.
     */
    void initialize();

private:
    /**
     * Bind API to Lua.
     */
    void bind_api();

    /**
     * Bind keyboard event.
     * @param appmode application mode for binding (viewer/slideshow/galler).
     * @param key text key description
     * @param cb keyboard handler (Lua callback function)
     */
    void bind_key(AppMode* appmode, const std::string& key,
                  const luabridge::LuaRef* cb);

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
