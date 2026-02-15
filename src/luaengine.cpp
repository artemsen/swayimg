// SPDX-License-Identifier: MIT
// Lua integration.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "luaengine.hpp"

#include "application.hpp"
#include "log.hpp"

// clang-format off
// lua headers must be included in this order
#include <lua.hpp>
#include <luabridge/LuaBridge.h>
#include <luabridge/Vector.h>
#include <luabridge/UnorderedMap.h>
// clang-format on

#include <cstdlib>
#include <cstring>

/**
 * Get path to config file (init.lua).
 * @return path to initial config file
 */
static std::filesystem::path get_config_file()
{
    const std::pair<const char*, const char*> env_paths[] = {
        { "XDG_CONFIG_HOME", "swayimg"          },
        { "XDG_CONFIG_DIRS", "swayimg"          },
        { "HOME",            ".config/swayimg"  },
        { nullptr,           "/etc/xdg/swayimg" }
    };

    for (size_t i = 0; i < sizeof(env_paths) / sizeof(env_paths[0]); ++i) {
        std::filesystem::path path;

        const char* env = env_paths[i].first;
        if (env) {
            env = std::getenv(env);
            if (!env) {
                continue;
            }
            // use only the first directory if prefix is a list
            const char* delim = strchr(env, ':');
            if (!delim) {
                path = env;
            } else {
                path = std::string(env, delim - 1);
            }
        }
        path /= env_paths[i].second;
        path /= "init.lua";

        if (std::filesystem::exists(path)) {
            return std::filesystem::absolute(path).lexically_normal();
        }
    }

    return {};
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

void LuaEngine::initialize()
{
    // get path to config file
    const std::filesystem::path config_file = get_config_file();
    if (config_file.empty()) {
        Log::debug("User config not found, use default settings");
        return;
    } else {
        Log::debug("Load user config from {}", config_file.c_str());
    }

    // initialize lua
    lua_state = luaL_newstate();
    if (!lua_state) {
        Log::error("Unable to initialize Lua state");
        return;
    }
    luaL_openlibs(lua_state);

    // add config dir to lua runtime path
    lua_getglobal(lua_state, "package");
    lua_getfield(lua_state, -1, "path");
    std::string pack_path = lua_tostring(lua_state, -1);
    lua_pop(lua_state, 2);
    pack_path += ";" + config_file.parent_path().string() + "/?.lua";
    lua_getglobal(lua_state, "package");
    lua_pushstring(lua_state, pack_path.c_str());
    lua_setfield(lua_state, -2, "path");
    lua_pop(lua_state, 1); // Pop package table

    // register lua bindings
    bind_api();

    // load config file
    if (luaL_loadfile(lua_state, config_file.c_str()) != LUA_OK) {
        Log::error("Failed to load config file: {}",
                   lua_tostring(lua_state, -1));
        return;
    }
    if (lua_pcall(lua_state, 0, 0, 0) != LUA_OK) {
        Log::error("Failed to execute config file: {}",
                   lua_tostring(lua_state, -1));
    }
}

void LuaEngine::bind_api()
{
    // clang-format off
    luabridge::getGlobalNamespace(lua_state)

    ////////////////////////////////////////////////////////////////////////////
    // global application control
    ////////////////////////////////////////////////////////////////////////////
    .beginNamespace("swayimg")
    .addFunction("set_title", [](const char* title) {
            Application::get_ui()->set_title(title); })
    .addFunction("exit", [](const std::optional<int> code = std::nullopt) {
            Application::self().exit(code ? *code : 0); })

    ////////////////////////////////////////////////////////////////////////////
    // font control
    ////////////////////////////////////////////////////////////////////////////
    .beginNamespace("font")
    .addProperty("name",
        []() { return Application::get_font().get_face(); },
        [](const std::string& name) { Application::get_font().set_face(name); })
    .addProperty("size",
        []() { return Application::get_font().get_size(); },
        [](const size_t size) { Application::get_font().set_size(size); })
    .endNamespace()

    ////////////////////////////////////////////////////////////////////////////
    // text layer control
    ////////////////////////////////////////////////////////////////////////////
    .beginNamespace("text")
    .addFunction("scheme_tl", [](const luabridge::LuaRef& table) {
            Application::get_text().set_scheme(Text::Position::TopLeft,
                table.cast<std::vector<std::string>>().value()); })
    // .addProperty("tr", []() { return nullptr; },
    //     [](const luabridge::LuaRef& table) {
    //         Application::get_text().set_scheme(Text::Position::TopRight,
    //             table.cast<std::vector<std::string>>().value()); })
    // .addProperty("bl", []() { return nullptr; },
    //     [](const luabridge::LuaRef& table) {
    //         Application::get_text().set_scheme(Text::Position::BottomLeft,
    //             table.cast<std::vector<std::string>>().value()); })
    // .addProperty("br", []() { return nullptr; },
    //     [](const luabridge::LuaRef& table) {
    //         Application::get_text().set_scheme(Text::Position::BottomRight,
    //             table.cast<std::vector<std::string>>().value()); })
    .endNamespace()

    ////////////////////////////////////////////////////////////////////////////
    // viewer mode
    ////////////////////////////////////////////////////////////////////////////
    .beginNamespace("view")
    .addFunction("bind_reset", []() { Application::get_viewer().bind_reset(); })
    .addFunction("bind_key",
        [this](const std::string& key, const luabridge::LuaRef& cb) {
            bind_key(&Application::get_viewer(), key, &cb);
        })
    .addFunction("on_open",
        [this](const luabridge::LuaRef& cb) {
        if (!cb.isFunction()) {
            Log::error("Invalid parameter, expected function, but got {}",
                       cb.tostring().c_str());
            return;
        }
        luabridge::LuaRef* ref = add_ref(&cb);
        Application::get_viewer().subscribe([ref](const ImagePtr& image) {
                std::unordered_map<std::string, std::string> params;
                params.insert(std::make_pair("path", image->entry->path));
                params.insert(std::make_pair("index", std::to_string(image->entry->index)));
                params.insert(std::make_pair("size", std::to_string(image->entry->size)));
                params.insert(std::make_pair("mtime", std::to_string(image->entry->mtime)));
                params.insert(std::make_pair("frames", std::to_string(image->frames.size())));
                (*ref)(params);
                });
        })
    .endNamespace()

    ////////////////////////////////////////////////////////////////////////////
    // slideshow mode
    ////////////////////////////////////////////////////////////////////////////
    .beginNamespace("slideshow")
    .addFunction("bind_reset", []() { Application::get_slideshow().bind_reset(); })
    .addFunction("bind_key",
        [this](const std::string& key, const luabridge::LuaRef& cb) {
            bind_key(&Application::get_slideshow(), key, &cb);
        })
    .endNamespace()

    ////////////////////////////////////////////////////////////////////////////
    // gallery mode
    ////////////////////////////////////////////////////////////////////////////
    .beginNamespace("gallery")
    .addFunction("bind_reset", []() { Application::get_gallery().bind_reset(); })
    .addFunction("bind_key",
        [this](const std::string& key, const luabridge::LuaRef& cb) {
            bind_key(&Application::get_gallery(), key, &cb);
        })
    .endNamespace()

    .endNamespace();
    // clang-format on
}

void LuaEngine::bind_key(AppMode* appmode, const std::string& key,
                         const luabridge::LuaRef* cb)
{
    std::optional<InputKeyboard> input = InputKeyboard::load(key);
    if (!input) {
        Log::error("Invalid binding key {}", key);
        return;
    }
    if (!cb->isFunction()) {
        Log::error("Invalid binding, expected function, but got {}",
                   cb->tostring().c_str());
        return;
    }

    luabridge::LuaRef* ref = add_ref(cb);
    appmode->bind_input(*input, [ref]() {
        (*ref)();
    });
}

luabridge::LuaRef* LuaEngine::add_ref(const luabridge::LuaRef* obj)
{
    luabridge::LuaRef* ref = new luabridge::LuaRef(*obj);
    refs.push_back(ref);
    return ref;
}
