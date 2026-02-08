// SPDX-License-Identifier: MIT
// Thread pool.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "threadpool.hpp"

#include "log.hpp"

#include <algorithm>

// Thread pool limits
constexpr size_t MIN_THREADS = 1;
constexpr size_t MAX_THREADS = 8;

ThreadPool::ThreadPool(const size_t threads)
{
    num_threads = threads;
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        num_threads = std::max(MIN_THREADS, num_threads);
        num_threads = std::min(MAX_THREADS, num_threads);
    }
    workers.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back(&ThreadPool::run, this);
    }
    Log::debug("Thread pool initialized for {} threads", num_threads);
}

ThreadPool::~ThreadPool()
{
    stop = true;
    tnotify.notify_all();
    for (auto& it : workers) {
        it.join();
    }
}

void ThreadPool::wait(const size_t tid)
{
    auto completed = [this, tid]() {
        auto it =
            std::find_if(tasks.begin(), tasks.end(), [&tid](const Task& task) {
                return task.id == tid;
            });
        return it == tasks.end() && !current.contains(tid);
    };

    std::unique_lock lock(mutex);
    if (completed()) {
        return;
    }
    complete.wait(lock, [completed]() {
        return completed();
    });
}

void ThreadPool::wait(const std::vector<size_t>& tids)
{
    for (auto& it : tids) {
        wait(it);
    }
}

void ThreadPool::run()
{
    while (!stop) {
        std::unique_lock lock(mutex);
        tnotify.wait(lock, [this]() {
            return !tasks.empty() || stop;
        });
        if (stop) {
            break;
        }
        if (!tasks.empty()) {
            Task task = std::move(tasks.front());
            tasks.pop_front();
            current.insert(task.id);
            lock.unlock();

            task.executor();

            lock.lock();
            current.erase(task.id);
            lock.unlock();

            complete.notify_all();
        }
    }
}
