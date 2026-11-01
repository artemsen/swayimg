// SPDX-License-Identifier: MIT
// Lua engine.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "luaengine.hpp"

#include <array>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace {

/**
 * Get path to config file (init.lua).
 * @return path to initial config file
 */
std::filesystem::path get_config_file()
{
    static constexpr std::array env_paths =
        std::to_array<std::pair<const char*, const char*>>({
            { "XDG_CONFIG_HOME", "swayimg"          },
            { "XDG_CONFIG_DIRS", "swayimg"          },
            { "HOME",            ".config/swayimg"  },
            { nullptr,           "/etc/xdg/swayimg" },
    });

    for (auto [env_name, postfix] : env_paths) {
        std::filesystem::path path;

        if (env_name) {
            const char* env = std::getenv(env_name);
            if (!env) {
                continue;
            }
            // use only the first directory if prefix is a list
            const char* delim = strchr(env, ':');
            if (!delim) {
                path = env;
            } else {
                path = std::string(env, delim);
            }
        }
        path /= postfix;
        path /= "init.lua";

        if (std::filesystem::exists(path)) {
            return std::filesystem::absolute(path).lexically_normal();
        }
    }

    return {};
}

} // anonymous namespace

LuaEngine::LuaEngine()
{
    lua_state = luaL_newstate();
    if (lua_state) {
        luaL_openlibs(lua_state);
    } else {
        Log::error("Unable to initialize Lua state");
    }
}

LuaEngine::~LuaEngine()
{
    if (lua_state) {
        for (auto& it : refs) {
            delete it;
        }
        lua_close(lua_state);
    }
}

bool LuaEngine::load(const std::filesystem::path& config)
{
    if (!lua_state) {
        return false;
    }

    const std::filesystem::path config_file =
        config.empty() ? get_config_file() : config;
    if (config_file.empty()) {
        Log::verbose("User config not found, use default settings");
        return true;
    }

    Log::verbose("Load user config from {}", config_file.c_str());

    // add config dir to lua runtime path
    const std::filesystem::path config_dir = config_file.parent_path();
    std::string pack_path = config_dir / "?.lua;";
    pack_path += config_dir / "?.so;";
    lua_getglobal(lua_state, "package");
    lua_getfield(lua_state, -1, "path");
    pack_path += lua_tostring(lua_state, -1);
    lua_pop(lua_state, 2);
    lua_getglobal(lua_state, "package");
    lua_pushstring(lua_state, pack_path.c_str());
    lua_setfield(lua_state, -2, "path");
    lua_pop(lua_state, 1); // Pop package table

    // load config file
    if (luaL_loadfile(lua_state, config_file.c_str()) != LUA_OK) {
        const char* msg = lua_tostring(lua_state, -1);
        print_error("Failed to load config file: {}", msg ? msg : "<?>");
    } else {
        const luabridge::LuaRef chunk =
            luabridge::LuaRef::fromStack(lua_state, -1);
        lua_pop(lua_state, 1);
        call(chunk);
    }

    return true;
}

void LuaEngine::execute(const std::string& script)
{
    assert(lua_state);

    if (luaL_loadstring(lua_state, script.c_str()) != LUA_OK) {
        const char* msg = lua_tostring(lua_state, -1);
        print_error("Failed to load script line: {}", msg ? msg : "<?>");
    } else {
        const luabridge::LuaRef chunk =
            luabridge::LuaRef::fromStack(lua_state, -1);
        lua_pop(lua_state, 1);
        call(chunk);
    }
}

const luabridge::LuaRef* LuaEngine::add_ref(const luabridge::LuaRef* ref)
{
    if (!ref->isFunction()) {
        raise_error("Invalid argument: expected function, but got {}",
                    ref->tostring());
    }

    const luabridge::LuaRef* fn = new luabridge::LuaRef(*ref);
    refs.push_back(fn);
    return fn;
}

void LuaEngine::remove_ref(const luabridge::LuaRef* ref)
{
    const auto it = std::find(refs.begin(), refs.end(), ref);
    assert(it != refs.end());
    if (it != refs.end()) {
        delete ref;
        refs.erase(it);
    }
}

std::string& LuaEngine::traceback_ref()
{
    static std::string traceback;
    return traceback;
}

int LuaEngine::traceback_lua(lua_State* lua)
{
    std::string& traceback = traceback_ref();
    traceback.clear();

    const char* msg = lua_tostring(lua, 1);

    // retrieve Lua's global 'debug.traceback' function
    lua_getglobal(lua, "debug");
    lua_getfield(lua, -1, "traceback");
    lua_remove(lua, -2);

    // push the original message and call level 2 (skipping this handler frame)
    lua_pushstring(lua, msg);
    lua_pushinteger(lua, 2);

    // call debug.traceback(message, 2)
    lua_call(lua, 2, 1);

    // traceback string is now at the top of the stack; save it for call_lua
    if (lua_isstring(lua, -1)) {
        size_t len = 0;
        const char* str = lua_tolstring(lua, -1, &len);

        // strip C function frames (e.g. "[C]: in function 'x'"), they only
        // refer to internal native API and are of no interest to the user
        const std::string_view source(str, len);
        traceback.reserve(source.size());
        size_t line_start = 0;
        while (line_start < source.size()) {
            const size_t line_end = source.find('\n', line_start);
            const std::string_view line = source.substr(
                line_start,
                line_end == std::string_view::npos ? source.size() - line_start
                                                   : line_end - line_start);

            if (line.find("[C]:") == std::string_view::npos) {
                traceback.append(line);
                if (line_end != std::string_view::npos) {
                    traceback.push_back('\n');
                }
            }

            if (line_end == std::string_view::npos) {
                break;
            }
            line_start = line_end + 1;
        }
    }

    return 1;
}

LuaBind::LuaBind(LuaEngine& lua)
    : lua(lua)
{
}

void LuaBind::init(const char* name)
{
    luabridge::Namespace ns = luabridge::getGlobalNamespace(lua);
    ns = ns.beginNamespace(ROOT_NS);
    if (name) {
        ns = ns.beginNamespace(name);
    }
    ns_ptr = &ns;

    bind();

    if (name) {
        ns = ns.endNamespace();
    }
    ns.endNamespace();
    ns_ptr = nullptr;
}
