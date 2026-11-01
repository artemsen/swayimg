// SPDX-License-Identifier: MIT
// Lua engine.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "log.hpp"
#include "text.hpp"

#include <lua.hpp>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress"
#pragma GCC diagnostic ignored "-Wnonnull-compare"
#endif
#include <luabridge/LuaBridge.h>
#include <luabridge/UnorderedMap.h>
#include <luabridge/Vector.h>
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <filesystem>
#include <format>
#include <string>
#include <vector>

/** Lua integration. */
class LuaEngine {
public:
    LuaEngine();
    ~LuaEngine();

    /**
     * Load config.
     * @param config path to the config file, can be empty to autodetect
     * @return true if Lua initialized successfully
     */
    bool load(const std::filesystem::path& config);

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
            const std::string& traceback = traceback_ref();
            print_error("{}", traceback.empty() ? "Lua error" : traceback);
        }
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
     * Add function reference to Lua object.
     * @param ref origin Lua reference to create
     * @return created reference to use it in future
     */
    const luabridge::LuaRef* add_ref(const luabridge::LuaRef* ref);

    /**
     * Remove reference from Lua object.
     * @param ref Lua reference to free
     */
    void remove_ref(const luabridge::LuaRef* ref);

    /**
     * Type-casting operator to a native Lua handle.
     * @return lua handle
     */
    operator lua_State*() const { return lua_state; }

private:
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
     * Get reference to the last Lua traceback message.
     * @return traceback message reference
     */
    static std::string& traceback_ref();

    /**
     * Handler used to get traceback of Lua call.
     * Called by Lua BEFORE the call stack unwinds.
     */
    static int traceback_lua(lua_State* lua);

private:
    lua_State* lua_state;                       ///< Lua state
    std::vector<const luabridge::LuaRef*> refs; ///< Own Lua references
};

/** Lua binding. */
class LuaBind {
public:
    /**
     * Constructor.
     * @param lua Lua engine instance
     */
    LuaBind(LuaEngine& lua);

    /**
     * Initialize bindings.
     * @param name name of the nested namespace
     */
    void init(const char* name);

protected:
    /**
     * Register a class member function pair as a read/write property.
     * The member functions are bound to this object via captured lambdas.
     * @param name property name
     * @param getter member getter function
     * @param setter member setter function
     */
    template <typename C, typename R, typename... Args>
    void add_property(const char* name, R (C::*getter)(),
                      void (C::*setter)(Args...))
    {
        ns_ptr->addProperty(
            name,
            [this, getter] {
                return (static_cast<C*>(this)->*getter)();
            },
            [this, setter](const Args&... args) {
                (static_cast<C*>(this)->*setter)(args...);
            });
    }

    /**
     * Register a class member setter as a write-only property.
     * The member function is bound to this object via a captured lambda,
     * reading the property raises an error.
     * @param name property name
     * @param setter member setter function
     */
    template <typename C, typename... Args>
    void add_property_wo(const char* name, void (C::*setter)(Args...))
    {
        ns_ptr->addProperty(
            name,
            [this, name] {
                lua.raise_error("Property {} is write-only", name);
            },
            [this, setter](const Args&... args) {
                (static_cast<C*>(this)->*setter)(args...);
            });
    }

    /**
     * Register a class member getter as a read-only property.
     * The member function is bound to this object via a captured lambda,
     * writing to the property raises an error.
     * @param name property name
     * @param getter member getter function
     */
    template <typename C, typename R>
    void add_property_ro(const char* name, R (C::*getter)())
    {
        ns_ptr->addProperty(name, [this, getter] {
            return (static_cast<C*>(this)->*getter)();
        });
    }

    /**
     * Register a class member function as a namespace function.
     * The member function is bound to this object via a captured lambda.
     * @param name function name
     * @param fn member function to bind
     */
    template <typename C, typename R, typename... Args>
    void add_function(const char* name, R (C::*fn)(Args...))
    {
        ns_ptr->addFunction(name, [this, fn](Args... args) -> R {
            return (static_cast<C*>(this)->*fn)(std::forward<Args>(args)...);
        });
    }

    /**
     * Bind properties and functions.
     */
    virtual void bind() = 0;

protected:
    /** Root namespace name. */
    static constexpr const char* ROOT_NS = "swayimg";

    // Using 0xff in the alpha channel in 32-bit systems leads to runtime
    // errors: "The native integer can't fit inside a lua integer".
    // Use double to avoid such errors.
#if defined(__ILP32__) || defined(_ILP32)
    using luacolor_t = double;
#else
    using luacolor_t = uint32_t;
#endif

protected:
    LuaEngine& lua; ///< Lua engine instance

private:
    luabridge::Namespace* ns_ptr = nullptr; ///< Lua namespace descriptor
};
