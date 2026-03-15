// SPDX-License-Identifier: MIT
// Thread pool.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "threadpool.hpp"

#include <algorithm>

// Thread pool limits
constexpr size_t MIN_THREADS = 1;
constexpr size_t MAX_THREADS = 8;

ThreadPool::ThreadPool(const size_t max_threads)
{
    assert(max_threads <= MAX_THREADS);

    threads =
        std::clamp(static_cast<size_t>(std::thread::hardware_concurrency()),
                   MIN_THREADS, max_threads ? max_threads : MAX_THREADS);
    start();
}

ThreadPool::~ThreadPool()
{
    stop();
}

void ThreadPool::wait()
{
    std::unique_lock lock(mutex);
    if (active.empty() && tasks.empty()) {
        return;
    }
    complete.wait(lock, [this]() {
        return active.empty() && tasks.empty();
    });
}

void ThreadPool::wait(const size_t tid)
{
    auto completed = [this, tid]() {
        if (active.contains(tid)) {
            return false;
        }
        for (const auto& it : tasks) {
            if (it.id == tid) {
                return false;
            }
        }
        return true;
    };

    std::unique_lock lock(mutex);
    if (completed()) {
        return;
    }
    complete.wait(lock, [&completed]() {
        return completed();
    });
}

void ThreadPool::wait(const std::vector<size_t>& tids)
{
    for (const auto& it : tids) {
        wait(it);
    }
}

void ThreadPool::cancel()
{
    const std::unique_lock lock(mutex);
    tasks.clear();
}

void ThreadPool::start()
{
    assert(workers.empty());
    assert(tasks.empty());
    assert(active.empty());

    quit = false;

    workers.reserve(threads);
    for (size_t i = 0; i < threads; ++i) {
        workers.emplace_back(&ThreadPool::run, this);
    }
}

void ThreadPool::stop()
{
    cancel();

    quit = true;
    tnotify.notify_all();
    complete.notify_all();

    for (auto& it : workers) {
        it.join();
    }
    workers.clear();
}

void ThreadPool::run()
{
    while (!quit) {
        std::unique_lock lock(mutex);
        tnotify.wait(lock, [this]() {
            return !tasks.empty() || quit;
        });
        if (quit) {
            break;
        }
        if (!tasks.empty()) {
            const Task task = std::move(tasks.front());
            tasks.pop_front();
            active.insert(task.id);
            lock.unlock();

            task.executor();

            lock.lock();
            active.erase(task.id);
            lock.unlock();
            complete.notify_all();
        }
    }
}
