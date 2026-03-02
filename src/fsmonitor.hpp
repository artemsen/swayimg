// SPDX-License-Identifier: MIT
// File system monitor.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <filesystem>
#include <map>

struct inotify_event;

/** File system monitor. */
class FsMonitor {
public:
    /**
     * Get global instance of file system monitor.
     * @return file system monitor instance
     */
    static FsMonitor& self();

    ~FsMonitor();

    /**
     * Initialize FS monitor.
     */
    void initialize();

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
    int fd = -1; ///< inotify file descriptor

    /** Watch descriptors linked to path. */
    std::map<int, std::filesystem::path> watch;
};
