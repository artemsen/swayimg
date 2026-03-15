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
     * Print verbose message.
     * @param fmt format description
     * @param ... format arguments
     */
    template <typename... Args>
    static void verbose(const std::format_string<Args...> fmt, Args&&... args)
    {
        if (verbose_enable()) {
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
    static bool& verbose_enable()
    {
        static bool verbose = false;
        return verbose;
    }

    /** Performance measurer. */
    struct PerfTimer {
        PerfTimer()
        {
            if (verbose_enable()) {
                clock_gettime(CLOCK_MONOTONIC, &begin_time);
            }
        }

        /**
         * Get elapsed time in seconds.
         * @return elapsed time
         */
        [[nodiscard]] float time() const
        {
            if (begin_time.tv_sec || begin_time.tv_nsec) {
                timespec end_time;
                clock_gettime(CLOCK_MONOTONIC, &end_time);
                const size_t ns =
                    (end_time.tv_sec * 1000000000 + end_time.tv_nsec) -
                    (begin_time.tv_sec * 1000000000 + begin_time.tv_nsec);
                return static_cast<double>(ns) / 1000000000;
            }
            return 0.0f;
        }

        timespec begin_time = {};
    };
};
