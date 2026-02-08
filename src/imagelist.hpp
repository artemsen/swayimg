// SPDX-License-Identifier: MIT
// List of images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "fsmonitor.hpp"

#include <ctime>
#include <filesystem>
#include <functional>
#include <list>
#include <shared_mutex>
#include <vector>

/** Thread-safe list of images. */
class ImageList {
public:
    /** Order of the image list. */
    enum class Order : uint8_t {
        None,    ///< Unsorted (system depended)
        Alpha,   ///< Lexicographic sort
        Numeric, ///< Numeric sort
        Mtime,   ///< Modification time sort
        Size,    ///< Size sort
        Random   ///< Random order
    };

    /** Position of the next entry. */
    enum class Pos : uint8_t {
        First,      ///< First entry in the list
        Last,       ///< Last entry in the list
        Next,       ///< Next entry
        Prev,       ///< Previous entry
        NextParent, ///< Next entry with different parent
        PrevParent, ///< Previous entry with different parent
        Random      ///< Random entry
    };

    /** Image entry in the list. */
    struct Entry {
        std::filesystem::path path; ///< Path to the image file
        std::time_t mtime;          ///< File modification time
        size_t size;                ///< Size of the image file
        size_t index;               ///< Image index

        /**
         * Check if the entry is valid.
         * @return true if image entry valid
         */
        bool valid() const;

        // File name used for image, that is read from stdin through pipe
        static constexpr const char* SRC_STDIN = "stdin://";
        // Special prefix used to load images from external command output
        static constexpr const char* SRC_EXEC = "exec://";
    };
    using EntryPtr = std::shared_ptr<Entry>;

    using FsEvent =
        std::function<void(const FsMonitor::Event, const EntryPtr&)>;

    /**
     * Initialize image list.
     * @param handler FS event handler
     */
    void initialize(const FsEvent& handler);

    /**
     * Load image list from specified sources.
     * @param sources list of sources to load
     * @return first entry from source list
     */
    EntryPtr load(const std::vector<std::filesystem::path>& sources);

    /**
     * Load image list from text file.
     * @param list_file path to the file to load
     * @return first entry from source list
     */
    EntryPtr load(const std::filesystem::path& list_file);

    /**
     * Add file or special source to the list.
     * @param path path to the file or special source
     */
    void add(const std::filesystem::path& path);

    /**
     * Remove image entry from the list.
     * @param entry entry to remove
     * @param forward direction of nearest returned entry
     * @return valid nearest entry or nullptr if list is empty
     */
    EntryPtr remove(const EntryPtr& entry, bool forward = true);

    /**
     * Get number of entries in the image list.
     * @return image list size
     */
    size_t size() const { return entries.size(); }

    /**
     * Find image entry by source path.
     * @param path path to the file or special source
     * @return image entry or nullptr if entry not found
     */
    EntryPtr find(const std::filesystem::path& path);

    /**
     * Get next entry from the list.
     * @param from starting image entry
     * @param pos next entry position
     * @return image entry or nullptr if entry not found
     */
    EntryPtr get(const EntryPtr& from, const Pos pos);

    /**
     * Get image entry in specified distance from the start.
     * @param from starting image entry
     * @param distance number entries to skip
     * @return image entry or nullptr if distance if out of bounds
     */
    EntryPtr get(const EntryPtr& from, const ssize_t distance);

    /**
     * Get distance between two image entries.
     * @param from,to image entries
     * @return number of entries in range [start,end]
     */
    ssize_t distance(const EntryPtr& from, const EntryPtr& to);

private:
    /**
     * Add file or special source to the list.
     * @param path path to the file or special source
     * @param ordered flag to add new entry to ordered position in the list
     */
    void add(const std::filesystem::path& path, const bool ordered);

    /**
     * Add files from the directory to the list.
     * @param path path to the directory
     * @param ordered flag to add new entry to ordered position in the list
     * @return array with added entries
     */
    std::vector<EntryPtr> add_dir(const std::filesystem::path& path,
                                  const bool ordered);

    /**
     * Add file to the list.
     * @param path path to the file
     * @param ordered flag to add new entry to ordered position in the list
     * @return added entry
     */
    EntryPtr add_file(const std::filesystem::path& path, const bool ordered);

    /**
     * Add new entry to the list.
     * @param entry image entry to add
     * @param ordered flag to add new entry to ordered position in the list
     */
    void add_entry(EntryPtr& entry, const bool ordered);

    /**
     * Sort image list.
     */
    void sort();

    /**
     * Reindex the image list.
     */
    void reindex();

    /**
     * Handle FS monitor event.
     * @param event FS event type
     * @param path modified path
     */
    void handle_fs(const FsMonitor::Event event,
                   const std::filesystem::path& path);

private:
    std::list<EntryPtr> entries; ///< List of image entries
    std::shared_mutex mutex;     ///< Image list mutex

    FsMonitor fs_mon; ///< File system monitor
    FsEvent fs_evh;   ///< File system event handler

public:
    Order order = Order::Numeric; ///< Image list order
    bool reverse = false;         ///< Reverse order flag

    bool recursive = false; ///< Read directories recursively
    bool adjacent = false;  ///< Open adjacent files from the same directory
};
