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

static void
iterate_dir(const std::function<void(const std::filesystem::path&)>& action,
            const std::filesystem::path& source, const bool recursive,
            const bool fsmon)
{
    if (fsmon) {
        FsMonitor::self().add(source);
    }
    try {
        for (const auto& it : std::filesystem::directory_iterator(source)) {
            if (std::filesystem::is_directory(it)) {
                if (recursive) {
                    iterate_dir(action, it, true, fsmon);
                }
            } else if (std::filesystem::is_regular_file(it)) {
                action(it);
            }
        }
    } catch (const std::filesystem::filesystem_error&) {
    }
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
    entries_arr.reserve(sources.size());
    for (auto& it : sources) {
        add(it, false);
    }
    sort();
    mutex.unlock();

    // disable loading of adjacent files, otherwise fs mon will add unnecessary
    // files to the list
    adjacent = false;

    // get first entry of the source list (not of the contents of the sources)
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
                std::find_if(entries_arr.begin(), entries_arr.end(),
                             [&abs_path](const ImageEntryPtr& entry) {
                                 return abs_path == entry->path.parent_path();
                             });
            if (it != entries_arr.end()) {
                first = *it;
                break;
            }
        }
    }

    if (!first && !entries_arr.empty()) {
        first = entries_arr.front();
    }

    return first;
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

std::list<ImageEntryPtr> ImageList::add(const std::filesystem::path& path)
{
    const std::unique_lock lock(mutex);
    return add(path, true);
}

ImageEntryPtr ImageList::remove(const ImageEntryPtr& entry, const bool forward)
{
    assert(entry);

    Log::verbose("Remove image entry {}", entry->path.filename().string());

    ImageEntryPtr next = get(entry, forward ? Dir::Next : Dir::Prev);

    const std::unique_lock lock(mutex);

    entries_map.erase(entry->path);
    entries_arr.erase(entries_arr.begin() + entry->index);
    reindex(entry->index);

    entry->remove();

    return next;
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
        const std::unique_lock lock(mutex);
        sort();
    }
}

void ImageList::set_reverse(const bool enable)
{
    if (reverse != enable) {
        reverse = enable;
        const std::unique_lock lock(mutex);
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

std::vector<ImageEntry> ImageList::get_all()
{
    const std::shared_lock lock(mutex);

    std::vector<ImageEntry> copy;
    copy.reserve(entries_arr.size());
    for (const auto& it : entries_arr) {
        copy.emplace_back(*it);
    }

    return copy;
}

ImageEntryPtr ImageList::get(const ImageEntryPtr& from, const Dir dir)
{
    assert(from || dir == Dir::First || dir == Dir::Last);

    const std::shared_lock lock(mutex);

    if (entries_arr.empty()) {
        return nullptr;
    }

    // handle removed entry: return nearest entry
    if (from && !*from) {
        assert(dir == Dir::Next || dir == Dir::Prev);
        const auto it = std::find_if(entries_arr.begin(), entries_arr.end(),
                                     [&from, this](const ImageEntryPtr& entry) {
                                         const bool cmp = compare_entries(
                                             *from, *entry, order);
                                         return reverse ? !cmp : cmp;
                                     });
        return it == entries_arr.end() ? entries_arr.front() : *it;
    }

    ImageEntryPtr entry = nullptr;

    switch (dir) {
        case Dir::First:
            entry = entries_arr.front();
            break;
        case Dir::Last:
            entry = entries_arr.back();
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
        case Dir::NextParent: {
            const std::filesystem::path from_parent = from->path.parent_path();
            for (size_t i = from->index + 1; i < entries_arr.size(); ++i) {
                if (from_parent != entries_arr[i]->path.parent_path()) {
                    entry = entries_arr[i];
                    break;
                }
            }
        } break;
        case Dir::PrevParent: {
            const std::filesystem::path from_parent = from->path.parent_path();
            for (ssize_t i = static_cast<ssize_t>(from->index) - 1; i >= 0;
                 --i) {
                if (from_parent != entries_arr[i]->path.parent_path()) {
                    entry = entries_arr[i];
                    break;
                }
            }
        } break;
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

    assert(from && *from);

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

    assert(from && *from);
    assert(from->index < entries_arr.size());
    assert(to && *to);
    assert(to->index < entries_arr.size());

    return static_cast<ssize_t>(to->index) - static_cast<ssize_t>(from->index);
}

std::list<ImageEntryPtr> ImageList::add(const std::filesystem::path& path,
                                        const bool ordered)
{
    if (ImageEntry::is_special(path)) {
        const ImageEntryPtr entry = add_special_source(path, ordered);
        if (entry) {
            return { entry };
        }
        return {};
    }

    std::filesystem::path abs_path;
    try {
        abs_path = std::filesystem::absolute(path).lexically_normal();
    } catch (const std::filesystem::filesystem_error&) {
        Log::warning("Invalid path {}, skipped", path.string());
        return {};
    }
    if (!std::filesystem::exists(abs_path)) {
        Log::warning("File {} not found, skipped", abs_path.string());
        return {};
    }

    std::list<ImageEntryPtr> added;
    const bool is_dir = std::filesystem::is_directory(abs_path);
    if (is_dir || adjacent) {
        iterate_dir(
            [&](const std::filesystem::path& abs_path) {
                added.emplace_back(add_file(abs_path, false));
            },
            is_dir ? abs_path : abs_path.parent_path(), recursive, fsmon);

        if (!added.empty() && ordered) {
            sort();
        }
    } else if (!std::filesystem::is_regular_file(abs_path)) {
        Log::warning("File {} is not regular, skipped", abs_path.string());
    } else {
        added.emplace_back(add_file(abs_path, ordered));
    }
    return added;
}

ImageEntryPtr ImageList::add_file(const std::filesystem::path& path,
                                  const bool ordered)
{
    assert(path.is_absolute());

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
    if (!add_entry(entry, ordered)) {
        return nullptr;
    }
    if (fsmon) {
        FsMonitor::self().add(path);
    }
    return entry;
}

ImageEntryPtr ImageList::add_special_source(const std::filesystem::path& path,
                                            const bool ordered)
{
    assert(ImageEntry::is_special(path));

    ImageEntryPtr entry = std::make_shared<ImageEntry>();
    entry->path = path;
    entry->mtime = 0;
    entry->size = 0;
    entry->index = 0;
    if (add_entry(entry, ordered)) {
        return entry;
    }
    return nullptr;
}

bool ImageList::add_entry(const ImageEntryPtr& entry, const bool ordered)
{
    if (entries_map.contains(entry->path)) {
        return false; // already exists
    }

    // search the right place to insert new entry according to sort order
    EntriesArray::const_iterator pos = entries_arr.end();
    if (ordered && !entries_arr.empty()) {
        if (order == Order::Random) {
            pos = entries_arr.begin() + rand() % entries_arr.size();
        } else if (order != Order::None) {
            pos = std::find_if(entries_arr.begin(), entries_arr.end(),
                               [&entry, this](const ImageEntryPtr& it) {
                                   const bool cmp =
                                       compare_entries(*entry, *it, order);
                                   return reverse ? !cmp : cmp;
                               });
        }
    }

    // insert new entry
    entries_map.insert({ entry->path, entry });
    if (pos == entries_arr.end()) {
        entry->index = entries_arr.size();
        entries_arr.push_back(entry);
    } else {
        entry->index = (*pos)->index;
        entries_arr.insert(pos, entry);
        reindex(entry->index + 1); // reindex all entries after this one
    }

    return true;
}

void ImageList::sort()
{
    if (order == Order::None) {
        // nothing to do
    } else if (order == Order::Random) {
        // shuffle list
        EntriesArray tmp(entries_arr.begin(), entries_arr.end());
        static std::random_device rdev;
        static std::mt19937 engine(rdev());
        std::shuffle(tmp.begin(), tmp.end(), engine);
        entries_arr.assign(tmp.begin(), tmp.end());
    } else {
        std::sort(entries_arr.begin(), entries_arr.end(),
                  [this](const ImageEntryPtr& l, const ImageEntryPtr& r) {
                      const bool cmp = compare_entries(*l, *r, order);
                      return reverse ? !cmp : cmp;
                  });
    }

    reindex();
}

void ImageList::reindex(const size_t index)
{
    const size_t sz = entries_arr.size();
    for (size_t i = index; i < sz; ++i) {
        entries_arr[i]->index = i;
    }
}
