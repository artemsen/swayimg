// SPDX-License-Identifier: MIT
// File system monitor.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <filesystem>
#include <unordered_map>

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

    /**
     * Remove file or directory from monitor.
     * @param path watched path
     */
    void remove(const std::filesystem::path& path);

    /**
     * Remove all file from monitor.
     */
    void clear();

private:
    /**
     * Handle inotify event.
     * @param event inotify event
     */
    void handle_event(const inotify_event* event);

private:
    int fd = -1; ///< inotify file descriptor

    std::unordered_map<int, std::filesystem::path> fds;   ///< FD to path map
    std::unordered_map<std::filesystem::path, int> paths; ///< Path to FD map
};
