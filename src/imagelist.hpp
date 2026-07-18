// SPDX-License-Identifier: MIT
// List of images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.hpp"

#include <ctime>
#include <filesystem>
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

    /** Direction of the next entry. */
    enum class Dir : uint8_t {
        First,      ///< First entry in the list
        Last,       ///< Last entry in the list
        Next,       ///< Next entry
        Prev,       ///< Previous entry
        NextParent, ///< Next entry with different parent
        PrevParent, ///< Previous entry with different parent
        Random      ///< Random entry
    };

    using EntriesArray = std::vector<ImageEntryPtr>;
    using EntriesMap = std::unordered_map<std::filesystem::path, ImageEntryPtr>;

    /**
     * Get global instance of image list.
     * @return image list instance
     */
    static ImageList& self();

    /** Constructor. */
    ImageList();

    /**
     * Add all images in specified sources.
     * @param sources list of sources to load
     * @return list of added entries in source order
     */
    EntriesArray add(const std::vector<std::filesystem::path>& sources);

    /**
     * Remove all given paths from the list.
     * @param sources entries paths to remove
     * @return list of removed entries
     */
    EntriesArray remove(const std::vector<std::filesystem::path>& sources);

    /**
     * Remove image entry from the list.
     * @param entry entry to remove
     */
    void remove(const ImageEntryPtr& entry);

    /**
     * Clear image list.
     * @return list of removed entries
     */
    EntriesArray clear();

    /**
     * Get number of entries in the image list.
     * @return image list size
     */
    size_t size();

    /**
     * Get image list order.
     * @return order of the image list
     */
    Order get_order() const { return order; }

    /**
     * Set order and sort the image list.
     * @param new_order new order of the image list
     */
    void set_order(const Order new_order);

    /**
     * Enable/disable reverse order.
     * @param enable state to set
     */
    void set_reverse(const bool enable);

    /**
     * Find image entry by source path.
     * @param path path to the file or special source
     * @return image entry or nullptr if entry not found
     */
    ImageEntryPtr find(const std::filesystem::path& path);

    /**
     * Get array with copy of all entries in the list.
     * @return array with all entries
     */
    EntriesArray get_all();

    /**
     * Get next entry from the list.
     * @param from starting image entry
     * @param dir next entry direction
     * @return image entry or nullptr if entry not found
     */
    ImageEntryPtr get(const ImageEntryPtr& from, const Dir dir);

    /**
     * Get image entry in specified distance from the start.
     * @param from starting image entry
     * @param distance number entries to skip
     * @return image entry or nullptr if distance if out of bounds
     */
    ImageEntryPtr get(const ImageEntryPtr& from, const ssize_t distance);

    /**
     * Get distance between two image entries.
     * @param from,to image entries
     * @return number of entries in range [start,end]
     */
    ssize_t distance(const ImageEntryPtr& from, const ImageEntryPtr& to);

private:
    /**
     * Get child entries by directory path.
     * @param path parent directory
     * @return list of entries
     */
    EntriesArray get_child(const std::filesystem::path& path) const;

    /**
     * Get the nearest entry with different parent.
     * @param from starting image entry
     * @param forward direction of nearest returned entry
     * @return image entry or nullptr if entry not found
     */
    ImageEntryPtr get_diffparent(const ImageEntryPtr& from, const bool forward);

    /**
     * Add file, directory or special source to the list.
     * @param path path to the file or special source
     * @return list of added entries
     */
    EntriesArray add_any(const std::filesystem::path& path);

    /**
     * Add files from the directory to the list.
     * @param path path to the directory
     * @return list with added entries
     */
    EntriesArray add_dir(const std::filesystem::path& path);

    /**
     * Add file to the list.
     * @param path path to the file
     * @return added entry
     */
    ImageEntryPtr add_file(const std::filesystem::path& path);

    /**
     * Add new entry to the list.
     * @param path path to the image file
     * @param mtime file modification time
     * @param size size of the file
     * @return added entry (nullptr if already exists)
     */
    ImageEntryPtr add_entry(const std::filesystem::path& path,
                            const std::time_t mtime = 0, const size_t size = 0);

    /**
     * Sort and reindex image list.
     */
    void sort();

    /**
     * Sort specified image list.
     * @param entries image list to sort
     */
    void sort(EntriesArray& entries) const;

    /**
     * Reindex the image list.
     * @param index (0-based) to start reindexing from
     */
    void reindex(const size_t index = 0);

private:
    EntriesArray entries_arr; ///< Array of image entries
    EntriesMap entries_map;   ///< Map of path to image entries

    std::shared_mutex mutex; ///< Image list mutex

    Order order;  ///< Image list order
    bool reverse; ///< Reverse order flag

public:
    bool recursive; ///< Read directories recursively
    bool adjacent;  ///< Open adjacent files from the same directory
    bool fsmon;     ///< FS monitor usage
};
