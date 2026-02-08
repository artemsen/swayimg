// SPDX-License-Identifier: MIT
// List of images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "imagelist.hpp"

#include "log.hpp"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <limits>
#include <mutex>
#include <random>
#include <string>

// Image entry index used for removed entries.
constexpr size_t INVALID_INDEX = std::numeric_limits<size_t>::max();

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
static bool compare_entries(const ImageList::Entry& l,
                            const ImageList::Entry& r,
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

bool ImageList::Entry::valid() const
{
    return index != INVALID_INDEX;
}

void ImageList::initialize(const FsEvent& handler)
{
    fs_evh = handler;
    fs_mon.initialize(std::bind(&ImageList::handle_fs, this,
                                std::placeholders::_1, std::placeholders::_2));
}

ImageList::EntryPtr
ImageList::load(const std::vector<std::filesystem::path>& sources)
{
    if (sources.empty()) {
        return nullptr;
    }

    mutex.lock();
    for (auto& it : sources) {
        add(it, false);
    }
    sort();
    reindex();
    mutex.unlock();

    // get first entry
    EntryPtr first = nullptr;
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
                             [&abs_path](const EntryPtr& entry) {
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

ImageList::EntryPtr ImageList::load(const std::filesystem::path& list_file)
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

void ImageList::add(const std::filesystem::path& path)
{
    std::unique_lock lock(mutex);
    add(path, true);
    reindex();
}

ImageList::EntryPtr ImageList::remove(const EntryPtr& entry, bool forward)
{
    assert(entry && entry->valid());

    EntryPtr next = get(entry, forward ? Pos::Next : Pos::Prev);

    std::unique_lock lock(mutex);

    entries.remove(entry);
    entry->index = INVALID_INDEX;

    reindex();

    return next;
}

ImageList::EntryPtr ImageList::find(const std::filesystem::path& path)
{
    std::string search = path.string();
    if (search.empty()) {
        return nullptr;
    }
    if (!search.starts_with(Entry::SRC_STDIN) &&
        !search.starts_with(Entry::SRC_EXEC)) {
        search = std::filesystem::absolute(path).lexically_normal();
    }

    std::shared_lock lock(mutex);

    auto it = std::find_if(entries.begin(), entries.end(),
                           [&search](const EntryPtr& entry) {
                               return search == entry->path;
                           });
    return it == entries.end() ? nullptr : *it;
}

ImageList::EntryPtr ImageList::get(const EntryPtr& from, const Pos pos)
{
    std::shared_lock lock(mutex);

    if (pos == Pos::First) {
        return entries.empty() ? nullptr : entries.front();
    }
    if (pos == Pos::Last) {
        return entries.empty() ? nullptr : entries.back();
    }

    assert(from);

    if (pos == Pos::Random) {
        if (entries.size() <= 1) {
            return nullptr;
        }
        EntryPtr random = from;
        while (random == from) {
            const size_t distance = rand() % entries.size();
            auto it = entries.begin();
            std::advance(it, distance);
            random = *it;
        }
        return random;
    }

    std::list<EntryPtr>::const_iterator it = entries.end();
    if (from) {
        if (from->valid()) {
            it = std::find(entries.begin(), entries.end(), from);
        } else {
            it = std::find_if(entries.begin(), entries.end(),
                              [&from, this](const EntryPtr& entry) {
                                  const bool cmp =
                                      compare_entries(*from, *entry, order);
                                  return reverse ? !cmp : cmp;
                              });
        }
    }

    if (pos == Pos::Next) {
        return ++it == entries.end() ? nullptr : *it;
    }
    if (pos == Pos::Prev) {
        return --it == entries.end() ? nullptr : *it;
    }

    if (it == entries.end()) {
        return nullptr;
    }

    const std::filesystem::path from_parent = from->path.parent_path();
    if (pos == Pos::NextParent) {
        while (++it != entries.end()) {
            if (from_parent != (*it)->path.parent_path()) {
                return *it;
            }
        }
        return nullptr;
    }
    if (pos == Pos::PrevParent) {
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

ImageList::EntryPtr ImageList::get(const EntryPtr& from, const ssize_t distance)
{
    std::shared_lock lock(mutex);

    assert(from && from->valid());

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

ssize_t ImageList::distance(const EntryPtr& from, const EntryPtr& to)
{
    assert(from && from->valid());
    assert(to && to->valid());

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

void ImageList::add(const std::filesystem::path& path, const bool ordered)
{
    if (path.string().starts_with(Entry::SRC_STDIN) ||
        path.string().starts_with(Entry::SRC_EXEC)) {
        EntryPtr entry = std::make_shared<Entry>();
        entry->path = path;
        entry->mtime = 0;
        entry->size = 0;
        entry->index = 0;
        add_entry(entry, ordered);
        return;
    }

    if (!std::filesystem::exists(path)) {
        Log::warning("File {} not found, skipped", path.string());
        return;
    }

    const std::filesystem::path abs_path =
        std::filesystem::absolute(path).lexically_normal();
    if (std::filesystem::is_directory(path)) {
        add_dir(abs_path, ordered);
    } else {
        add_file(abs_path, ordered);
        if (adjacent) {
            add_dir(abs_path.parent_path(), ordered);
        }
    }
}

std::vector<ImageList::EntryPtr>
ImageList::add_dir(const std::filesystem::path& path, const bool ordered)
{
    std::vector<EntryPtr> entries;

    try {
        for (const auto& it : std::filesystem::directory_iterator(path)) {
            const std::filesystem::path& sub_path = it.path();
            if (std::filesystem::is_directory(sub_path)) {
                if (recursive) {
                    std::vector<EntryPtr> added = add_dir(sub_path, ordered);
                    entries.insert(entries.end(), added.begin(), added.end());
                }
            } else {
                EntryPtr entry = add_file(sub_path, ordered);
                if (entry) {
                    entries.push_back(entry);
                }
            }
        }
        fs_mon.add(path);
    } catch (const std::filesystem::filesystem_error&) {
    }

    return entries;
}

ImageList::EntryPtr ImageList::add_file(const std::filesystem::path& path,
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

    EntryPtr entry = std::make_shared<Entry>();
    entry->path = path;
    entry->mtime = tt_time;
    entry->size = std::filesystem::file_size(path);
    entry->index = 0;
    add_entry(entry, ordered);

    fs_mon.add(path);

    return entry;
}

void ImageList::add_entry(EntryPtr& entry, const bool ordered)
{
    const auto it = std::find_if(entries.begin(), entries.end(),
                                 [&entry](const EntryPtr& it) {
                                     return entry->path == it->path;
                                 });
    if (it != entries.end()) {
        Log::warning("File {} already exists in list, skipped",
                     entry->path.string());
        return;
    }

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
        auto it = std::find_if(
            entries.begin(), entries.end(), [&entry, this](const EntryPtr& it) {
                const bool cmp = compare_entries(*entry, *it, order);
                return reverse ? !cmp : cmp;
            });
        entries.insert(it, entry);
    }
}

void ImageList::sort()
{
    if (order == Order::None) {
        return;
    }

    if (order == Order::Random) {
        // shuffle list
        std::vector<EntryPtr> tmp(entries.begin(), entries.end());
        std::random_device rdev;
        std::mt19937 engine(rdev());
        std::shuffle(tmp.begin(), tmp.end(), engine);
        entries.assign(tmp.begin(), tmp.end());
        return;
    }

    entries.sort([this](const EntryPtr& l, const EntryPtr& r) {
        const bool cmp = compare_entries(*l, *r, order);
        return reverse ? !cmp : cmp;
    });

    reindex();
}

void ImageList::reindex()
{
    size_t index = 0;
    for (auto& it : entries) {
        it->index = ++index;
    }
}

void ImageList::handle_fs(const FsMonitor::Event event,
                          const std::filesystem::path& path)
{
    assert(path.is_absolute());
    const bool is_dir = std::filesystem::is_directory(path);

    switch (event) {
        case FsMonitor::Event::Create:
            if (is_dir) {
                if (recursive) {
                    mutex.lock();
                    const std::vector<EntryPtr> entries = add_dir(path, true);
                    reindex();
                    mutex.unlock();
                    for (auto& it : entries) {
                        fs_evh(event, it);
                    }
                }
            } else {
                mutex.lock();
                EntryPtr entry = add_file(path, true);
                if (entry) {
                    reindex();
                }
                mutex.unlock();
                if (entry) {
                    fs_evh(event, entry);
                }
            }
            break;
        case FsMonitor::Event::Remove:
            if (!is_dir) {
                EntryPtr entry = find(path);
                if (entry) {
                    fs_evh(event, entry);
                    remove(entry);
                }
            }
            break;
        case FsMonitor::Event::Modify:
            if (!is_dir) {
                EntryPtr entry = find(path);
                if (entry) {
                    fs_evh(event, entry);
                }
            }
            break;
    }
}
