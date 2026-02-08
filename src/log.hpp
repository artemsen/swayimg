// SPDX-License-Identifier: MIT
// Logging.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <cstring>
#include <format>
#include <iostream>

class Log {
public:
    /**
     * Print debug message.
     * @param fmt format description
     * @param ... format arguments
     */
    template <typename... Args>
    static void debug(const std::format_string<Args...> fmt, Args&&... args)
    {
        if (verbose_flag()) {
            std::cout << std::vformat(fmt.get(), std::make_format_args(args...))
                      << '\n';
        }
    }

    /**
     * Print informational message.
     * @param fmt format description
     * @param ... format arguments
     */
    template <typename... Args>
    static void info(const std::format_string<Args...> fmt, Args&&... args)
    {
        std::cout << std::vformat(fmt.get(), std::make_format_args(args...))
                  << '\n';
    }

    /**
     * Print warning message.
     * @param fmt format description
     * @param ... format arguments
     */
    template <typename... Args>
    static void warning(const std::format_string<Args...> fmt, Args&&... args)
    {
        std::cerr << "WARNING: "
                  << std::vformat(fmt.get(), std::make_format_args(args...))
                  << '\n';
    }

    /**
     * Print error message.
     * @param fmt format description
     * @param ... format arguments
     */
    template <typename... Args>
    static void error(const std::format_string<Args...> fmt, Args&&... args)
    {
        std::cerr << "ERROR: "
                  << std::vformat(fmt.get(), std::make_format_args(args...))
                  << '\n';
    }

    /**
     * Print error message.
     * @param code system error code
     * @param fmt format description
     * @param ... format arguments
     */
    template <typename... Args>
    static void error(int code, const std::format_string<Args...> fmt,
                      Args&&... args)
    {
        std::cerr << "ERROR: "
                  << std::vformat(fmt.get(), std::make_format_args(args...));
        if (code) {
            std::cerr << ", error code [" << code << "] "
                      << std::strerror(code);
        }
        std::cerr << '\n';
    }

    /**
     * Verbose output flag getter/setter.
     * @return reference to verbose flag
     */
    static bool& verbose_flag()
    {
        static bool verbose = false;
        return verbose;
    }
};
