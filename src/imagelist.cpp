// SPDX-License-Identifier: MIT
// List of images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "imagelist.hpp"

#include "application.hpp"
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
            size_t lnum = 0, rnum = 0;
            try {
                lnum = std::stoull(ls);
                rnum = std::stoull(rs);
            } catch (const std::out_of_range&) {
                // number too large, fall back to lexicographic compare
                cmp = coll.compare(ls.data(), ls.data() + ls.length(),
                                   rs.data(), rs.data() + rs.length());
                break;
            }
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

ImageList::~ImageList()
{
    scanning = false;
    scan_pool.stop();
}

ImageEntryPtr ImageList::load(const std::vector<std::filesystem::path>& sources)
{
    if (sources.empty()) {
        return nullptr;
    }

    std::vector<std::filesystem::path> bg_dirs;

    mutex.lock();
    for (auto& it : sources) {
        const std::string path_str = it.string();
        if (path_str.starts_with(ImageEntry::SRC_STDIN) ||
            path_str.starts_with(ImageEntry::SRC_EXEC)) {
            add(it, false);
        } else if (!std::filesystem::exists(it)) {
            Log::warning("File {} not found, skipped", path_str);
        } else if (std::filesystem::is_directory(it)) {
            const std::filesystem::path abs_path =
                std::filesystem::absolute(it).lexically_normal();
            // scan first level only (no recursion)
            try {
                for (const auto& dir_entry :
                     std::filesystem::directory_iterator(abs_path)) {
                    const std::filesystem::path& sub_path = dir_entry.path();
                    if (std::filesystem::is_directory(sub_path)) {
                        if (recursive) {
                            bg_dirs.push_back(sub_path);
                        }
                    } else {
                        add_file(sub_path, false);
                    }
                }
                FsMonitor::self().add(abs_path);
            } catch (const std::filesystem::filesystem_error&) {
            }
        } else {
            const std::filesystem::path abs_path =
                std::filesystem::absolute(it).lexically_normal();
            add_file(abs_path, false);
            if (adjacent) {
                try {
                    const auto parent = abs_path.parent_path();
                    for (const auto& dir_entry :
                         std::filesystem::directory_iterator(parent)) {
                        const auto& sub_path = dir_entry.path();
                        if (!std::filesystem::is_directory(sub_path)) {
                            add_file(sub_path, false);
                        }
                    }
                    FsMonitor::self().add(parent);
                } catch (const std::filesystem::filesystem_error&) {
                }
            }
        }
    }
    // if no files found at top level but we have subdirs to scan,
    // synchronously scan until we find at least one file
    if (entries.empty() && !bg_dirs.empty()) {
        std::vector<ImageEntryPtr> batch;
        while (entries.empty() && !bg_dirs.empty()) {
            const auto dir = bg_dirs.front();
            bg_dirs.erase(bg_dirs.begin());
            try {
                for (const auto& dir_entry :
                     std::filesystem::directory_iterator(dir)) {
                    const auto& sub_path = dir_entry.path();
                    if (std::filesystem::is_directory(sub_path)) {
                        bg_dirs.push_back(sub_path);
                    } else {
                        add_file(sub_path, false);
                    }
                }
                FsMonitor::self().add(dir);
            } catch (const std::filesystem::filesystem_error&) {
            }
        }
    }

    reindex();
    mutex.unlock();

    sort(true);

    // disable loading of adjacent files, otherwise fs mon will add unnecessary
    // files to the list
    adjacent = false;

    // get first entry
    std::shared_lock lock(mutex);
    if (entries.empty()) {
        return nullptr;
    }

    ImageEntryPtr first = nullptr;
    for (auto& path : sources) {
        first = find_unlocked(path);
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
    return first ? first : entries.front();
}

ImageEntryPtr ImageList::load(const std::filesystem::path& list_file)
{
    std::ifstream file(list_file);
    if (!file.is_open()) {
        Log::error("Unable to open file {}", list_file.string());
        return nullptr;
    }

    // Read all paths from file — fast path that bypasses the generic
    // load(sources) to avoid per-entry exists()/is_directory() branching
    // and the expensive sort on large lists.
    std::vector<std::filesystem::path> paths;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            paths.emplace_back(line);
        }
    }
    file.close();

    if (paths.empty()) {
        return nullptr;
    }

    // Add all entries synchronously under a single lock.
    // Skip sort — preserve the file list order (typically filesystem order
    // from fd/find, which is good enough for browsing).
    mutex.lock();
    for (const auto& p : paths) {
        const std::filesystem::path abs_path =
            p.is_absolute() ? p : std::filesystem::absolute(p).lexically_normal();
        if (std::filesystem::is_regular_file(abs_path)) {
            std::error_code ec;
            const auto fs_time = std::filesystem::last_write_time(abs_path, ec);
            if (ec) {
                continue;
            }
            auto sys_time =
                std::chrono::time_point_cast<
                    std::chrono::system_clock::duration>(
                    fs_time -
                    std::filesystem::file_time_type::clock::now() +
                    std::chrono::system_clock::now());

            ImageEntryPtr entry = std::make_shared<ImageEntry>();
            entry->path = abs_path;
            entry->mtime = std::chrono::system_clock::to_time_t(sys_time);
            entry->size = std::filesystem::file_size(abs_path, ec);
            entry->index = 0;

            const std::string path_str = abs_path.string();
            if (!path_index.count(path_str)) {
                entries.push_back(entry);
                path_index.emplace(path_str, std::prev(entries.end()));
            }
        }
    }
    reindex();
    mutex.unlock();

    // No sort — order::none semantics for file-list input.
    // No background scanning — all entries are explicit file paths.

    std::shared_lock lock(mutex);
    return entries.empty() ? nullptr : entries.front();
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

    std::unique_lock lock(mutex);

    ImageEntryPtr next = nullptr;
    auto pi = path_index.find(entry->path.string());
    if (pi != path_index.end()) {
        auto it = pi->second;
        if (forward) {
            auto nit = std::next(it);
            if (nit != entries.end()) {
                next = *nit;
            }
        } else {
            if (it != entries.begin()) {
                next = *std::prev(it);
            }
        }
        entries.erase(it);
        path_index.erase(pi);
    }
    entry->remove();

    reindex_needed = true;

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
    std::shared_lock lock(mutex);
    return find_unlocked(path);
}

ImageEntryPtr ImageList::find_unlocked(const std::filesystem::path& path)
{
    std::string search = path.string();
    if (search.empty()) {
        return nullptr;
    }
    if (!search.starts_with(ImageEntry::SRC_STDIN) &&
        !search.starts_with(ImageEntry::SRC_EXEC)) {
        search = std::filesystem::absolute(path).lexically_normal();
    }

    auto it = path_index.find(search);
    return it == path_index.end() ? nullptr : *(it->second);
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

    // O(1) lookup via path_index instead of O(n) linear search
    auto pi = path_index.find(from->path.string());
    if (pi == path_index.end()) {
        return nullptr;
    }
    auto it = pi->second;

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

    // O(1) lookup via path_index instead of O(n) linear search
    auto pi = path_index.find(from->path.string());
    if (pi == path_index.end()) {
        return nullptr;
    }
    auto it = pi->second;

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
    if (path.string().starts_with(ImageEntry::SRC_STDIN) ||
        path.string().starts_with(ImageEntry::SRC_EXEC)) {
        ImageEntryPtr entry = std::make_shared<ImageEntry>();
        entry->path = path;
        entry->mtime = 0;
        entry->size = 0;
        entry->index = 0;
        add_entry(entry, ordered);
        return { entry };
    }

    if (!std::filesystem::exists(path)) {
        Log::warning("File {} not found, skipped", path.string());
        return {};
    }

    const std::filesystem::path abs_path =
        std::filesystem::absolute(
            (std::filesystem::is_directory(path) || !adjacent)
                ? path
                : path.parent_path())
            .lexically_normal();

    if (std::filesystem::is_directory(abs_path)) {
        return add_dir(abs_path, ordered);
    } else {
        return { add_file(abs_path, ordered) };
    }
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

    return entry;
}

void ImageList::add_entry(ImageEntryPtr& entry, const bool ordered)
{
    const std::string path_str = entry->path.string();
    if (path_index.count(path_str)) {
        return; // already exists
    }

    Log::verbose("Add image entry {}", entry->path.filename().string());

    if (!ordered || order == Order::None) {
        entries.push_back(entry);
        path_index.emplace(path_str, std::prev(entries.end()));
    } else if (order == Order::Random) {
        if (entries.size() <= 1) {
            entries.push_back(entry);
            path_index.emplace(path_str, std::prev(entries.end()));
        } else {
            const size_t distance = rand() % entries.size();
            auto it = entries.begin();
            std::advance(it, distance);
            auto ins = entries.insert(it, entry);
            path_index.emplace(path_str, ins);
        }
    } else {
        // search the right place to insert new entry according to sort order
        auto it = std::find_if(entries.begin(), entries.end(),
                               [&entry, this](const ImageEntryPtr& it) {
                                   const bool cmp =
                                       compare_entries(*entry, *it, order);
                                   return reverse ? !cmp : cmp;
                               });
        auto ins = entries.insert(it, entry);
        path_index.emplace(path_str, ins);
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

    rebuild_path_index();
}

void ImageList::reindex()
{
    size_t index = 0;
    for (auto& it : entries) {
        it->index = ++index;
    }
}

void ImageList::rebuild_path_index()
{
    path_index.clear();
    path_index.reserve(entries.size());
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        path_index.emplace((*it)->path.string(), it);
    }
}

void ImageList::ensure_indexed()
{
    if (reindex_needed.exchange(false)) {
        std::unique_lock lock(mutex);
        reindex();
    }
}

bool ImageList::is_scanning() const
{
    return scanning.load();
}

void ImageList::scan_directory(const std::filesystem::path& path)
{
    if (!scanning.load()) {
        if (scan_active.fetch_sub(1) == 1) {
            finish_scan();
        }
        return;
    }

    std::vector<ImageEntryPtr> batch;
    batch.reserve(100);

    try {
        for (const auto& it : std::filesystem::directory_iterator(path)) {
            if (!scanning.load()) {
                break;
            }
            const std::filesystem::path& sub_path = it.path();
            if (std::filesystem::is_directory(sub_path)) {
                // enqueue subdirectory as a new task
                scan_active.fetch_add(1);
                scan_pool.add(&ImageList::scan_directory, this, sub_path);
            } else if (std::filesystem::is_regular_file(sub_path)) {
                const auto fs_time =
                    std::filesystem::last_write_time(sub_path);
                auto sys_time =
                    std::chrono::time_point_cast<
                        std::chrono::system_clock::duration>(
                        fs_time -
                        std::filesystem::file_time_type::clock::now() +
                        std::chrono::system_clock::now());
                const std::time_t tt_time =
                    std::chrono::system_clock::to_time_t(sys_time);

                ImageEntryPtr entry = std::make_shared<ImageEntry>();
                entry->path = sub_path;
                entry->mtime = tt_time;
                entry->size = std::filesystem::file_size(sub_path);
                entry->index = 0;
                batch.push_back(entry);
            }
        }
    } catch (const std::filesystem::filesystem_error&) {
    }

    if (!batch.empty()) {
        push_pending(batch);
        Application::self().add_event(
            AppEvent::ScanProgress { total_discovered.load() });
    }

    // register directory with FsMonitor on main thread
    {
        std::lock_guard lock(pending_mutex);
        pending_dirs.push_back(path);
    }

    if (scan_active.fetch_sub(1) == 1) {
        finish_scan();
    }
}

void ImageList::push_pending(std::vector<ImageEntryPtr>& batch)
{
    std::lock_guard lock(pending_mutex);
    total_discovered.fetch_add(batch.size());
    pending_entries.insert(pending_entries.end(),
                           std::make_move_iterator(batch.begin()),
                           std::make_move_iterator(batch.end()));
}

void ImageList::drain_pending()
{
    std::vector<ImageEntryPtr> to_insert;
    std::vector<std::filesystem::path> to_monitor;
    {
        std::lock_guard lock(pending_mutex);
        to_insert.swap(pending_entries);
        to_monitor.swap(pending_dirs);
    }

    if (!to_insert.empty()) {
        std::unique_lock lock(mutex);
        for (auto& entry : to_insert) {
            const std::string path_str = entry->path.string();
            if (!path_index.count(path_str)) {
                entries.push_back(entry);
                path_index.emplace(path_str, std::prev(entries.end()));
            }
        }
        reindex();
    }

    for (const auto& dir : to_monitor) {
        FsMonitor::self().add(dir);
    }
}

void ImageList::finish_scan()
{
    // drain any remaining pending entries
    drain_pending();

    {
        std::unique_lock lock(mutex);

        if (order == Order::None) {
            // nothing to do
        } else if (order == Order::Random) {
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

        reindex();
        rebuild_path_index();
        total_discovered = entries.size();
        scanning = false;
    }

    Application::self().add_event(AppEvent::ScanComplete {});
}
