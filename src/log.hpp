// SPDX-License-Identifier: MIT
// Logging.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <cstring>
#include <format>
#include <iostream>

namespace Log {

/**
 * Enable verbose output.
 */
void enable_verbose();

/**
 * Check if verbose output enabled.
 * @return verbose status
 */
bool verbose_enabled();

/**
 * Print debug message.
 * @param fmt format description
 * @param ... format arguments
 */
template <typename... Args>
inline void debug(const std::format_string<Args...> fmt, Args&&... args)
{
    if (verbose_enabled()) {
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
inline void info(const std::format_string<Args...> fmt, Args&&... args)
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
inline void warning(const std::format_string<Args...> fmt, Args&&... args)
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
inline void error(const std::format_string<Args...> fmt, Args&&... args)
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
inline void error(int code, const std::format_string<Args...> fmt,
                  Args&&... args)
{
    std::cerr << "ERROR: "
              << std::vformat(fmt.get(), std::make_format_args(args...));
    if (code) {
        std::cerr << ", error code [" << code << "] " << std::strerror(code);
    }
    std::cerr << '\n';
}

}; // namespace Log
