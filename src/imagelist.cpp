// SPDX-License-Identifier: MIT
// List of images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "imagelist.hpp"

#include "fsmonitor.hpp"
#include "log.hpp"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <mutex>
#include <random>
#include <string>

/**
 * Comparison of two localized strings.
 * @param l,r strings to compare
 * @param numeric flag to use numeric compare
 * @return compare result
 */
static int compare_strings(const std::string& l, const std::string& r,
                           const bool numeric)
{
    int cmp = 0;
    const std::locale loc("");
    const std::collate<char>& coll = std::use_facet<std::collate<char>>(loc);

    if (!numeric) {
        cmp = coll.compare(l.data(), l.data() + l.length(), r.data(),
                           r.data() + r.length());
    } else {
        const std::string digits = "0123456789";
        std::string ls(l);
        std::string rs(r);
        while (true) {
            const size_t ldstart = ls.find_first_of(digits);
            const size_t rdstart = rs.find_first_of(digits);
            if (ldstart == std::string::npos || rdstart != ldstart) {
                cmp = coll.compare(ls.data(), ls.data() + ls.length(),
                                   rs.data(), rs.data() + rs.length());
                break;
            }
            cmp = coll.compare(ls.data(), ls.data() + ldstart, rs.data(),
                               rs.data() + rdstart);
            if (cmp) {
                break;
            }

            ls.erase(0, ldstart);
            rs.erase(0, rdstart);
            const size_t lnum = std::stoull(ls);
            const size_t rnum = std::stoull(rs);
            if (lnum != rnum) {
                cmp = lnum < rnum ? -1 : 1;
                break;
            }

            const size_t ldend = ls.find_first_not_of(digits);
            const size_t rdend = rs.find_first_not_of(digits);
            ls.erase(0, ldend);
            rs.erase(0, rdend);
        }
    }

    return cmp;
}

/**
 * Comparison of two paths.
 * @param l,r paths to compare
 * @param numeric flag to use numeric compare
 * @return compare result
 */
static int compare_paths(const std::filesystem::path& l,
                         const std::filesystem::path& r, const bool numeric)
{
    // compare parents
    const std::filesystem::path l_parent = l.parent_path();
    const std::filesystem::path r_parent = r.parent_path();
    auto it_l = l_parent.begin();
    auto it_r = r_parent.begin();
    while (it_l != l_parent.end() && it_r != r_parent.end()) {
        const int cmp = compare_strings(*it_l, *it_r, numeric);
        if (cmp) {
            return cmp;
        }
        ++it_l;
        ++it_r;
    }
    if (it_l == l_parent.end() && it_r != r_parent.end()) {
        return -1;
    }
    if (it_l != l_parent.end() && it_r == r_parent.end()) {
        return 1;
    }

    // compare names
    return compare_strings(l.filename(), r.filename(), numeric);
}

/**
 * Comparison of two image entries.
 * @param l,r image entries to compare
 * @param order criterion for comparison
 * @return true if l < r
 */
static bool compare_entries(const ImageEntry& l, const ImageEntry& r,
                            const ImageList::Order order)
{
    switch (order) {
        case ImageList::Order::Alpha:
            return compare_paths(l.path, r.path, false) < 0;
        case ImageList::Order::Numeric:
            return compare_paths(l.path, r.path, true) < 0;
        case ImageList::Order::Mtime:
            return l.mtime < r.mtime;
        case ImageList::Order::Size:
            return l.size < r.size;
        case ImageList::Order::None:
        case ImageList::Order::Random:
            break;
    }
    assert(false); // unreachable
    return false;
}

ImageList& ImageList::self()
{
    static ImageList singleton;
    return singleton;
}

ImageEntryPtr ImageList::load(const std::vector<std::filesystem::path>& sources)
{
    if (sources.empty()) {
        return nullptr;
    }

    mutex.lock();
    for (auto& it : sources) {
        add(it, false);
    }
    mutex.unlock();

    sort(true);

    // disable loading of adjacent files, otherwise fs mon will add unnecessary
    // files to the list
    adjacent = false;

    // get first entry
    ImageEntryPtr first = nullptr;
    for (auto& path : sources) {
        first = find(path);
        if (first) {
            break;
        }
        if (std::filesystem::is_directory(path)) {
            const std::filesystem::path abs_path =
                std::filesystem::absolute(path).lexically_normal();
            auto it =
                std::find_if(entries.begin(), entries.end(),
                             [&abs_path](const ImageEntryPtr& entry) {
                                 return abs_path == entry->path.parent_path();
                             });
            if (it != entries.end()) {
                first = *it;
                break;
            }
        }
    }
    return first ? first : *entries.begin();
}

ImageEntryPtr ImageList::load(const std::filesystem::path& list_file)
{
    std::ifstream file(list_file);
    if (!file.is_open()) {
        Log::error("Unable to open file {}", list_file.string());
        return nullptr;
    }

    std::vector<std::filesystem::path> sources;

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            sources.emplace_back(line);
        }
    }
    file.close();

    return load(sources);
}

std::vector<ImageEntryPtr> ImageList::add(const std::filesystem::path& path)
{
    std::unique_lock lock(mutex);
    std::vector<ImageEntryPtr> entries = add(path, true);
    reindex();

    return entries;
}

ImageEntryPtr ImageList::remove(const ImageEntryPtr& entry, bool forward)
{
    assert(entry);

    Log::verbose("Remove image entry {}", entry->path.filename().string());

    ImageEntryPtr next = get(entry, forward ? Dir::Next : Dir::Prev);

    std::unique_lock lock(mutex);

    entries.remove(entry);
    entry->remove();

    reindex();

    return next;
}

void ImageList::set_order(const Order new_order)
{
    if (order != new_order || new_order == Order::Random) {
        order = new_order;
        sort(true);
    }
}

void ImageList::set_reverse(const bool enable)
{
    if (reverse != enable) {
        reverse = enable;
        sort(true);
    }
}

ImageEntryPtr ImageList::find(const std::filesystem::path& path)
{
    std::string search = path.string();
    if (search.empty()) {
        return nullptr;
    }
    if (!search.starts_with(ImageEntry::SRC_STDIN) &&
        !search.starts_with(ImageEntry::SRC_EXEC)) {
        search = std::filesystem::absolute(path).lexically_normal();
    }

    std::shared_lock lock(mutex);

    auto it = std::find_if(entries.begin(), entries.end(),
                           [&search](const ImageEntryPtr& entry) {
                               return search == entry->path;
                           });
    return it == entries.end() ? nullptr : *it;
}

std::vector<ImageEntry> ImageList::get_all()
{
    std::shared_lock lock(mutex);

    std::vector<ImageEntry> copy;
    copy.reserve(entries.size());
    for (const auto& it : entries) {
        copy.emplace_back(*it);
    }

    return copy;
}

ImageEntryPtr ImageList::get(const ImageEntryPtr& from, const Dir dir)
{
    std::shared_lock lock(mutex);

    if (entries.empty()) {
        return nullptr;
    }

    if (dir == Dir::First) {
        return entries.front();
    }
    if (dir == Dir::Last) {
        return entries.back();
    }

    assert(from);

    if (dir == Dir::Random) {
        if (entries.size() <= 1) {
            return nullptr;
        }
        ImageEntryPtr random = from;
        while (random == from) {
            const size_t distance = rand() % entries.size();
            auto it = entries.begin();
            std::advance(it, distance);
            random = *it;
        }
        return random;
    }

    // handle removed entry: return nearest entry
    if (!*from) {
        const auto it = std::find_if(entries.begin(), entries.end(),
                                     [&from, this](const ImageEntryPtr& entry) {
                                         const bool cmp = compare_entries(
                                             *from, *entry, order);
                                         return reverse ? !cmp : cmp;
                                     });
        return it == entries.end() ? entries.front() : *it;
    }

    auto it = std::find(entries.begin(), entries.end(), from);

    if (dir == Dir::Next) {
        return ++it == entries.end() ? nullptr : *it;
    }
    if (dir == Dir::Prev) {
        return --it == entries.end() ? nullptr : *it;
    }

    assert(it != entries.end());

    const std::filesystem::path from_parent = from->path.parent_path();
    if (dir == Dir::NextParent) {
        while (++it != entries.end()) {
            if (from_parent != (*it)->path.parent_path()) {
                return *it;
            }
        }
        return nullptr;
    }
    if (dir == Dir::PrevParent) {
        while (--it != entries.end()) {
            if (from_parent != (*it)->path.parent_path()) {
                return *it;
            }
        }
        return nullptr;
    }

    assert(false && "unhandled iterating position");
    return nullptr;
}

ImageEntryPtr ImageList::get(const ImageEntryPtr& from, const ssize_t distance)
{
    std::shared_lock lock(mutex);

    assert(from && *from);

    auto it = std::find(entries.begin(), entries.end(), from);
    assert(it != entries.end());

    ssize_t step = distance;
    if (step > 0) {
        while (it != entries.end() && step) {
            --step;
            ++it;
        }
    } else if (step < 0) {
        while (it != entries.end() && step) {
            ++step;
            --it;
        }
    }

    return it == entries.end() ? nullptr : *it;
}

ssize_t ImageList::distance(const ImageEntryPtr& from, const ImageEntryPtr& to)
{
    assert(from && *from);
    assert(to && *to);

    std::shared_lock lock(mutex);

    ssize_t distance = 0;
    bool forward = true;

    auto it_from = entries.end();
    auto it_to = entries.end();

    auto it = entries.begin();
    while (it != entries.end()) {
        if (*it == from) {
            it_from = it;
            if (it_to == entries.end()) {
                forward = true;
            }
        }
        if (*it == to) {
            it_to = it;
            if (it_from == entries.end()) {
                forward = false;
            }
        }
        ++it;
    }

    assert(it_from != entries.end());
    assert(it_to != entries.end());

    it = it_from;
    while (it != it_to) {
        if (forward) {
            ++distance;
            ++it;
        } else {
            --distance;
            --it;
        }
    }

    return distance;
}

std::vector<ImageEntryPtr> ImageList::add(const std::filesystem::path& path,
                                          const bool ordered)
{
    std::vector<ImageEntryPtr> entries;

    if (path.string().starts_with(ImageEntry::SRC_STDIN) ||
        path.string().starts_with(ImageEntry::SRC_EXEC)) {
        ImageEntryPtr entry = std::make_shared<ImageEntry>();
        entry->path = path;
        entry->mtime = 0;
        entry->size = 0;
        entry->index = 0;
        add_entry(entry, ordered);
        entries.push_back(entry);
    } else {
        if (!std::filesystem::exists(path)) {
            Log::warning("File {} not found, skipped", path.string());
        } else {
            const std::filesystem::path abs_path =
                std::filesystem::absolute(path).lexically_normal();
            if (std::filesystem::is_directory(path)) {
                entries = add_dir(abs_path, ordered);
            } else {
                entries.push_back(add_file(abs_path, ordered));
                if (adjacent) {
                    const std::vector<ImageEntryPtr> edir =
                        add_dir(abs_path.parent_path(), ordered);
                    entries.insert(entries.end(), edir.begin(), edir.end());
                }
            }
        }
    }

    return entries;
}

std::vector<ImageEntryPtr> ImageList::add_dir(const std::filesystem::path& path,
                                              const bool ordered)
{
    std::vector<ImageEntryPtr> entries;

    try {
        FsMonitor::self().add(path);
        for (const auto& it : std::filesystem::directory_iterator(path)) {
            const std::filesystem::path& sub_path = it.path();
            if (std::filesystem::is_directory(sub_path)) {
                if (recursive) {
                    std::vector<ImageEntryPtr> added = add_dir(sub_path, false);
                    entries.insert(entries.end(), added.begin(), added.end());
                }
            } else {
                ImageEntryPtr entry = add_file(sub_path, false);
                if (entry) {
                    entries.push_back(entry);
                }
            }
        }
    } catch (const std::filesystem::filesystem_error&) {
    }

    if (ordered) {
        sort(false);
    }

    return entries;
}

ImageEntryPtr ImageList::add_file(const std::filesystem::path& path,
                                  const bool ordered)
{
    assert(path.is_absolute());

    if (!std::filesystem::is_regular_file(path)) {
        Log::warning("File {} is not a regular, skipped", path.string());
        return nullptr;
    }

    const auto fs_time = std::filesystem::last_write_time(path);
    auto sys_time =
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            fs_time - std::filesystem::file_time_type::clock::now() +
            std::chrono::system_clock::now());
    const std::time_t tt_time = std::chrono::system_clock::to_time_t(sys_time);

    ImageEntryPtr entry = std::make_shared<ImageEntry>();
    entry->path = path;
    entry->mtime = tt_time;
    entry->size = std::filesystem::file_size(path);
    entry->index = 0;
    add_entry(entry, ordered);

    FsMonitor::self().add(path);

    return entry;
}

void ImageList::add_entry(ImageEntryPtr& entry, const bool ordered)
{
    if (std::find_if(entries.begin(), entries.end(),
                     [&entry](const ImageEntryPtr& it) {
                         return entry->path == it->path;
                     }) != entries.end()) {
        return; // already exists
    }

    Log::verbose("Add image entry {}", entry->path.filename().string());

    if (!ordered || order == Order::None) {
        entries.push_back(entry);
    } else if (order == Order::Random) {
        if (entries.size() <= 1) {
            entries.push_back(entry);
        } else {
            const size_t distance = rand() % entries.size();
            auto it = entries.begin();
            std::advance(it, distance);
            entries.insert(it, entry);
        }
    } else {
        // search the right place to insert new entry according to sort order
        auto it = std::find_if(entries.begin(), entries.end(),
                               [&entry, this](const ImageEntryPtr& it) {
                                   const bool cmp =
                                       compare_entries(*entry, *it, order);
                                   return reverse ? !cmp : cmp;
                               });
        entries.insert(it, entry);
    }
}

void ImageList::sort(bool locked)
{
    auto unlocked_sort = [this]() {
        if (order == Order::None) {
            // nothing to do
        } else if (order == Order::Random) {
            // shuffle list
            std::vector<ImageEntryPtr> tmp(entries.begin(), entries.end());
            std::random_device rdev;
            std::mt19937 engine(rdev());
            std::shuffle(tmp.begin(), tmp.end(), engine);
            entries.assign(tmp.begin(), tmp.end());
        } else {
            entries.sort(
                [this](const ImageEntryPtr& l, const ImageEntryPtr& r) {
                    const bool cmp = compare_entries(*l, *r, order);
                    return reverse ? !cmp : cmp;
                });
        }
    };

    if (locked) {
        std::unique_lock lock(mutex);
        unlocked_sort();
        reindex();
    } else {
        unlocked_sort();
        reindex();
    }
}

void ImageList::reindex()
{
    size_t index = 0;
    for (auto& it : entries) {
        it->index = ++index;
    }
}
