// SPDX-License-Identifier: MIT
// File system monitor.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <filesystem>
#include <functional>
#include <map>

struct inotify_event;

/** File system monitor. */
class FsMonitor {
public:
    ~FsMonitor();

    /** File system event types. */
    enum class Event : uint8_t {
        Create,
        Modify,
        Remove,
    };

    using Callback =
        std::function<void(const Event, const std::filesystem::path&)>;

    /**
     * Initialize FS monitor.
     * @param cb event handler
     */
    void initialize(const Callback& cb);

    /**
     * Register file or directory in monitor.
     * @param path watched path
     */
    void add(const std::filesystem::path& path);

private:
    /**
     * Handle inotify event.
     * @param event inotify event
     */
    void handle_event(const inotify_event* event);

private:
    int fd = -1;      ///< inotify file descriptor
    Callback handler; ///< Event handler

    /** Watch descriptors linked to path. */
    std::map<int, std::filesystem::path> watch;
};
