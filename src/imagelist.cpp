// SPDX-License-Identifier: MIT
// List of images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "imagelist.hpp"

#include "defaults.hpp"
#include "fsmonitor.hpp"
#include "log.hpp"

#include <algorithm>
#include <cassert>
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
    static const std::locale loc("");
    static const std::collate<char>& coll =
        std::use_facet<std::collate<char>>(loc);

    int cmp = 0;

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

            try {
                const size_t lnum = std::stoull(ls);
                const size_t rnum = std::stoull(rs);
                if (lnum != rnum) {
                    cmp = lnum < rnum ? -1 : 1;
                    break;
                }
            } catch (const std::out_of_range&) {
                cmp = coll.compare(l.data(), l.data() + l.length(), r.data(),
                                   r.data() + r.length());
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
            return l.mtime == r.mtime ? compare_paths(l.path, r.path, false) < 0
                                      : l.mtime < r.mtime;
        case ImageList::Order::Size:
            return l.size == r.size ? compare_paths(l.path, r.path, false) < 0
                                    : l.size < r.size;
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

ImageList::ImageList()
    : order(Defaults::imglist::order)
    , reverse(Defaults::imglist::reverse)
    , recursive(Defaults::imglist::recursive)
    , adjacent(Defaults::imglist::adjacent)
    , fsmon(Defaults::imglist::fsmon)
{
}

ImageList::EntriesArray
ImageList::add(const std::vector<std::filesystem::path>& sources)
{
    EntriesArray added;
    const std::scoped_lock lock(mutex);
    for (const auto& path : sources) {
        EntriesArray entries = add_any(path);
        added.reserve(added.size() + entries.size());
        added.insert(added.end(), entries.begin(), entries.end());
    }

    sort();

    return added;
}

ImageList::EntriesArray
ImageList::remove(const std::vector<std::filesystem::path>& sources)
{
    if (sources.empty()) {
        return {};
    }

    const std::scoped_lock lock(mutex);

    if (entries_arr.empty()) {
        return {};
    }

    EntriesArray removed;

    for (const auto& path : sources) {
        // get absolute path
        std::filesystem::path abs_path;
        if (ImageEntry::is_special(path)) {
            abs_path = path;
        } else {
            try {
                abs_path = std::filesystem::absolute(path).lexically_normal();
            } catch (const std::filesystem::filesystem_error&) {
                continue;
            }
        }

        if (std::filesystem::is_directory(abs_path)) {
            // remove all child entries
            for (const ImageEntryPtr& entry : get_child(abs_path)) {
                entry->removed = true;
                entries_map.erase(entry->path);
                entries_arr[entry->index] = nullptr;
                removed.emplace_back(entry);
            }
            FsMonitor::self().remove(abs_path);
        } else {
            // remove single entry
            auto it = entries_map.find(abs_path);
            if (it != entries_map.end()) {
                const ImageEntryPtr entry = it->second;
                entry->removed = true;
                entries_map.erase(it);
                entries_arr[entry->index] = nullptr;
                removed.emplace_back(entry);
            }
        }
    }

    if (!removed.empty()) {
        FsMonitor& fsmon = FsMonitor::self();
        for (const ImageEntryPtr& entry : removed) {
            fsmon.remove(entry->path);
        }
        std::erase_if(entries_arr, [](const ImageEntryPtr& entry) {
            return !entry;
        });
        reindex();
    }

    return removed;
}

ImageEntryPtr ImageList::remove(const ImageEntryPtr& entry, const bool forward)
{
    assert(entry);

    ImageEntryPtr next = get(entry, forward ? Dir::Next : Dir::Prev);

    const std::scoped_lock lock(mutex);

    entries_map.erase(entry->path);
    entries_arr.erase(entries_arr.begin() + entry->index);
    reindex(entry->index);

    entry->removed = true;
    FsMonitor::self().remove(entry->path);

    return next;
}

ImageList::EntriesArray ImageList::clear()
{
    const std::scoped_lock lock(mutex);

    EntriesArray removed = entries_arr;
    entries_map.clear();
    entries_arr.clear();

    FsMonitor::self().clear();

    return removed;
}

size_t ImageList::size()
{
    const std::shared_lock lock(mutex);
    return entries_arr.size();
}

void ImageList::set_order(const Order new_order)
{
    if (order != new_order || new_order == Order::Random) {
        order = new_order;
        const std::scoped_lock lock(mutex);
        sort();
    }
}

void ImageList::set_reverse(const bool enable)
{
    if (reverse != enable) {
        reverse = enable;
        const std::scoped_lock lock(mutex);
        sort();
    }
}

ImageEntryPtr ImageList::find(const std::filesystem::path& path)
{
    std::string search = path.string();
    if (search.empty()) {
        return nullptr;
    }
    if (!ImageEntry::is_special(search)) {
        try {
            search = std::filesystem::absolute(path).lexically_normal();
        } catch (const std::filesystem::filesystem_error&) {
            return nullptr;
        }
    }

    const std::shared_lock lock(mutex);

    auto it = entries_map.find(search);
    return it == entries_map.end() ? nullptr : it->second;
}

ImageList::EntriesArray ImageList::get_all()
{
    const std::shared_lock lock(mutex);
    return entries_arr;
}

ImageEntryPtr ImageList::get(const ImageEntryPtr& from, const Dir dir)
{
    const std::shared_lock lock(mutex);

    if (entries_arr.empty()) {
        return nullptr;
    }
    if (dir == Dir::First) {
        return entries_arr.front();
    }
    if (dir == Dir::Last) {
        return entries_arr.back();
    }

    assert(from);

    // handle removed entry: return nearest entry
    if (from->removed) {
        size_t index = from->index;
        if (index &&
            (dir == ImageList::Dir::Prev ||
             dir == ImageList::Dir::PrevParent)) {
            --index;
        }
        index = std::min(index, entries_arr.size() - 1);
        return entries_arr[index];
    }

    ImageEntryPtr entry = nullptr;

    switch (dir) {
        case Dir::First:
        case Dir::Last:
            assert(false && "should be already handled");
            break;
        case Dir::Next:
            if (from->index + 1 < entries_arr.size()) {
                entry = entries_arr[from->index + 1];
            }
            break;
        case Dir::Prev:
            if (entries_arr.size() > 1 && from->index > 0) {
                entry = entries_arr[from->index - 1];
            }
            break;
        case Dir::NextParent:
            entry = get_diffparent(from, true);
            break;
        case Dir::PrevParent:
            entry = get_diffparent(from, false);
            break;
        case Dir::Random:
            if (entries_arr.size() > 1) {
                entry = from;
                while (entry == from) {
                    entry = entries_arr[rand() % entries_arr.size()];
                }
            }
            break;
    }

    return entry;
}

ImageEntryPtr ImageList::get(const ImageEntryPtr& from, const ssize_t distance)
{
    const std::shared_lock lock(mutex);

    assert(from && !from->removed);

    const size_t index = from->index;
    if (index + distance >= entries_arr.size() ||
        static_cast<ssize_t>(index) + distance < 0) {
        return nullptr;
    }

    return entries_arr[index + distance];
}

ssize_t ImageList::distance(const ImageEntryPtr& from, const ImageEntryPtr& to)
{
    const std::shared_lock lock(mutex);

    assert(from && !from->removed);
    assert(from->index < entries_arr.size());
    assert(to && !to->removed);
    assert(to->index < entries_arr.size());

    return static_cast<ssize_t>(to->index) - static_cast<ssize_t>(from->index);
}

ImageList::EntriesArray
ImageList::get_child(const std::filesystem::path& path) const
{
    assert(path.is_absolute());

    EntriesArray child;

    for (const ImageEntryPtr& entry : entries_arr) {
        const std::filesystem::path rel = entry->path.lexically_relative(path);
        if (!rel.empty() && !rel.string().starts_with("..")) {
            child.push_back(entry);
        }
    }

    return child;
}

ImageEntryPtr ImageList::get_diffparent(const ImageEntryPtr& from,
                                        const bool forward)
{
    assert(from && !from->removed);

    const std::filesystem::path from_parent = from->path.parent_path();

    const ssize_t size = static_cast<ssize_t>(entries_arr.size());
    const ssize_t direction_sign = forward ? 1 : -1;
    ssize_t index = static_cast<ssize_t>(from->index) + direction_sign;

    while (index >= 0 && index < size) {
        ImageEntryPtr entry = entries_arr[index];
        if (from_parent != entry->path.parent_path()) {
            return entry;
        }
        index += direction_sign;
    }

    return nullptr;
}

ImageList::EntriesArray ImageList::add_any(const std::filesystem::path& path)
{
    if (ImageEntry::is_special(path)) {
        const ImageEntryPtr entry = add_entry(path);
        if (entry) {
            return { entry };
        }
        return {};
    }

    std::filesystem::path abs_path;
    try {
        abs_path = std::filesystem::absolute(path).lexically_normal();
        if (!std::filesystem::exists(abs_path)) {
            Log::warning("File {} not found, skipped", abs_path.string());
            return {};
        }
    } catch (const std::filesystem::filesystem_error&) {
        Log::warning("Invalid path {}, skipped", path.string());
        return {};
    }

    EntriesArray added;
    if (!std::filesystem::is_directory(abs_path)) {
        const ImageEntryPtr entry = add_file(abs_path);
        if (entry) {
            added.emplace_back(entry);
        }

        if (!adjacent) {
            if (fsmon) {
                FsMonitor::self().add(abs_path);
            }
            return added;
        }
        abs_path = abs_path.parent_path();
    }

    EntriesArray dir_entries = add_dir(abs_path);
    if (!dir_entries.empty()) {
        sort(dir_entries);
        added.reserve(added.size() + dir_entries.size());
        added.insert(added.end(), dir_entries.begin(), dir_entries.end());
    }

    return added;
}

ImageList::EntriesArray ImageList::add_dir(const std::filesystem::path& path)
{
    EntriesArray added;

    if (fsmon) {
        FsMonitor::self().add(path);
    }

    try {
        for (const auto& it : std::filesystem::directory_iterator(path)) {
            const std::filesystem::path& child_path = it.path();
            if (std::filesystem::is_directory(child_path)) {
                if (recursive) {
                    EntriesArray entries = add_dir(child_path);
                    added.reserve(added.size() + entries.size());
                    added.insert(added.end(), entries.begin(), entries.end());
                }
            } else {
                const ImageEntryPtr entry = add_file(child_path);
                if (entry) {
                    added.push_back(entry);
                }
            }
        }
    } catch (const std::filesystem::filesystem_error&) {
    }

    return added;
}

ImageEntryPtr ImageList::add_file(const std::filesystem::path& path)
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
    const std::time_t mtime = std::chrono::system_clock::to_time_t(sys_time);

    const size_t size = std::filesystem::file_size(path);

    return add_entry(path, mtime, size);
}

ImageEntryPtr ImageList::add_entry(const std::filesystem::path& path,
                                   const std::time_t mtime, const size_t size)
{
    ImageEntryPtr entry = nullptr;

    if (!entries_map.contains(path)) {
        entry = std::make_shared<ImageEntry>();
        entry->path = path;
        entry->mtime = mtime;
        entry->size = size;
        entries_map.insert({ entry->path, entry });
        entries_arr.push_back(entry);
    }

    return entry;
}

void ImageList::sort()
{
    sort(entries_arr);
    reindex();
}

void ImageList::sort(EntriesArray& entries) const
{
    if (order == Order::None) {
        // nothing to do
    } else if (order == Order::Random) {
        // shuffle list
        EntriesArray tmp(entries.begin(), entries.end());
        static std::random_device rdev;
        static std::mt19937 engine(rdev());
        std::shuffle(tmp.begin(), tmp.end(), engine);
        entries.assign(tmp.begin(), tmp.end());
    } else {
        std::sort(entries.begin(), entries.end(),
                  [this](const ImageEntryPtr& l, const ImageEntryPtr& r) {
                      const bool cmp = compare_entries(*l, *r, order);
                      return reverse ? !cmp : cmp;
                  });
    }
}

void ImageList::reindex(const size_t index)
{
    const size_t sz = entries_arr.size();
    for (size_t i = index; i < sz; ++i) {
        entries_arr[i]->index = i;
    }
}
