// SPDX-License-Identifier: MIT
// List of images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.hpp"

#include <ctime>
#include <filesystem>
#include <list>
#include <shared_mutex>
#include <unordered_set>
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

    /**
     * Get global instance of image list.
     * @return image list instance
     */
    static ImageList& self();

    /**
     * Load image list from specified sources.
     * @param sources list of sources to load
     * @return first entry from source list
     */
    ImageEntryPtr load(const std::vector<std::filesystem::path>& sources);

    /**
     * Load image list from text file.
     * @param list_file path to the file to load
     * @return first entry from source list
     */
    ImageEntryPtr load(const std::filesystem::path& list_file);

    /**
     * Add file or special source to the list.
     * @param path path to the file or special source
     * @return array of added entries
     */
    std::vector<ImageEntryPtr> add(const std::filesystem::path& path);

    /**
     * Remove image entry from the list.
     * @param entry entry to remove
     * @param forward direction of nearest returned entry
     * @return valid nearest entry or nullptr if list is empty
     */
    ImageEntryPtr remove(const ImageEntryPtr& entry, bool forward = true);

    /**
     * Get number of entries in the image list.
     * @return image list size
     */
    size_t size() const { return entries.size(); }

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
    std::vector<ImageEntry> get_all();

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
     * Add file or special source to the list.
     * @param path path to the file or special source
     * @param ordered flag to add new entry to ordered position in the list
     * @return array of added entries
     */
    std::vector<ImageEntryPtr> add(const std::filesystem::path& path,
                                   const bool ordered);

    /**
     * Add files from the directory to the list.
     * @param path path to the directory
     * @param ordered flag to add new entry to ordered position in the list
     * @return array with added entries
     */
    std::vector<ImageEntryPtr> add_dir(const std::filesystem::path& path,
                                       const bool ordered);

    /**
     * Add file to the list.
     * @param path path to the file
     * @param ordered flag to add new entry to ordered position in the list
     * @return added entry
     */
    ImageEntryPtr add_file(const std::filesystem::path& path,
                           const bool ordered);

    /**
     * Add new entry to the list.
     * @param entry image entry to add
     * @param ordered flag to add new entry to ordered position in the list
     */
    void add_entry(ImageEntryPtr& entry, const bool ordered);

    /**
     * Sort image list.
     * @param locked flag to lock the list before processing
     */
    void sort(bool locked);

    /**
     * Reindex the image list.
     */
    void reindex();

private:
    std::list<ImageEntryPtr> entries; ///< List of image entries
    std::shared_mutex mutex;          ///< Image list mutex

    /** Set of paths used for searching duplicates. */
    std::unordered_set<std::filesystem::path> duplicates;

    Order order = Order::Numeric; ///< Image list order
    bool reverse = false;         ///< Reverse order flag

public:
    bool recursive = false; ///< Read directories recursively
    bool adjacent = false;  ///< Open adjacent files from the same directory
    bool fsmon = true;      ///< FS monitor usage
};
