// SPDX-License-Identifier: MIT
// Logging.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <cstring>
#include <format>
#include <iostream>
#include <string>

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
            const std::string msg = sanitize(
                std::vformat(fmt.get(), std::make_format_args(args...)));
            std::cout << msg << '\n';
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
        const std::string msg =
            sanitize(std::vformat(fmt.get(), std::make_format_args(args...)));
        std::cout << msg << '\n';
    }

    /**
     * Print warning message.
     * @param fmt format description
     * @param ... format arguments
     */
    template <typename... Args>
    static void warning(const std::format_string<Args...> fmt, Args&&... args)
    {
        const std::string msg = "WARNING: " +
            sanitize(std::vformat(fmt.get(), std::make_format_args(args...)));
        std::cerr << msg << '\n';
    }

    /**
     * Print error message.
     * @param fmt format description
     * @param ... format arguments
     */
    template <typename... Args>
    static void error(const std::format_string<Args...> fmt, Args&&... args)
    {
        const std::string msg = "ERROR: " +
            sanitize(std::vformat(fmt.get(), std::make_format_args(args...)));
        std::cerr << msg << '\n';
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
        std::string msg = "ERROR: " +
            sanitize(std::vformat(fmt.get(), std::make_format_args(args...)));
        if (code) {
            msg +=
                std::format(", error code [{}] {}", code, std::strerror(code));
        }
        std::cerr << msg << '\n';
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

private:
    /**
     * Strip terminal escape sequences and control characters from a log
     * message. Prevents OSC injection (e.g. clipboard write via OSC 52),
     * title manipulation (OSC 0), cursor position report (CSI 6n), and
     * other terminal attacks through crafted filenames.
     * @param msg the formatted log message
     * @return sanitized copy safe for terminal output
     */
    static std::string sanitize(const std::string& msg)
    {
        const size_t len = msg.length();
        std::string result;
        result.reserve(len);

        for (size_t i = 0; i < len; ++i) {
            const unsigned char ch = msg[i];
            if (ch == 0x1b) {
                ++i; // ESC: skip the entire sequence
                if (i < len && msg[i] == '[') {
                    // CSI: ESC [ <params> <final byte 0x40-0x7E>
                    for (++i; i < len && msg[i] < 0x40; ++i) {}
                } else if (i < len && msg[i] == ']') {
                    // OSC: ESC ] ... (BEL or ST)
                    for (++i; i < len && msg[i] != 0x07 &&
                         !(msg[i] == 0x1b && i + 1 < len && msg[i + 1] == '\\');
                         ++i) {}
                    if (i < len && msg[i] == 0x1b) {
                        ++i; // skip the \ of ST
                    }
                } else if (i < len && msg[i] >= 0x40 && msg[i] <= 0x5f) {
                    // other C1: skip introducer
                }
            } else if (ch < 0x20 && ch != '\n') {
                // control character: drop
            } else if (ch == 0x7f) {
                // DEL: drop
            } else {
                result += msg[i];
            }
        }

        return msg;
    }
};
