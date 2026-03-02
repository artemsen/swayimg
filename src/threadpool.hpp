// SPDX-License-Identifier: MIT
// Thread pool.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

/** Thread pool. */
class ThreadPool {
public:
    /** Task description. */
    struct Task {
        size_t id;                      ///< Task id
        std::function<void()> executor; ///< Task function
    };

    /**
     * Constructor.
     * @param threads max number of threads in pool
     */
    ThreadPool(const size_t max_threads = 0);

    ~ThreadPool();

    /**
     * Get number of threads in the pool.
     * @return number of threads
     */
    inline size_t size() const { return threads; }

    /**
     * Add task to execution queue.
     * @param fn worker function to handle a task
     * @param args function arguments
     * @return task id
     */
    template <typename F, typename... Args> size_t add(F&& fn, Args&&... args)
    {
        assert(!quit);

        size_t task_id;
        auto task_fn =
            std::bind(std::forward<F>(fn), std::forward<Args>(args)...);

        mutex.lock();
        task_id = ++last_id;
        tasks.emplace_back(Task { task_id, task_fn });
        mutex.unlock();

        tnotify.notify_one();

        return task_id;
    }

    /**
     * Wait all tasks to complete.
     */
    void wait();

    /**
     * Wait for specified task to complete.
     * @param tid task id for waiting
     */
    void wait(const size_t tid);

    /**
     * Wait for specified tasks to complete.
     * @param tids tasks id for waiting
     */
    void wait(const std::vector<size_t>& tids);

    /**
     * Cancel all queued tasks.
     */
    void cancel();

    /**
     * Start worker threads.
     */
    void start();

    /**
     * Cancel all tasks and stop worker threads.
     */
    void stop();

private:
    /**
     * Thread worker function.
     */
    void run();

private:
    size_t threads;                   ///< Size of the poll (number of threads)
    size_t last_id = 0;               ///< Last task id
    std::vector<std::thread> workers; ///< Array of threads

    std::deque<Task> tasks;          ///< Task queue
    std::condition_variable tnotify; ///< Task queue notification
    std::mutex mutex;                ///< Task queue mutex
    std::atomic<bool> quit;          ///< Stop execution flag

    std::set<size_t> active;          ///< Set of currently executing tasks
    std::condition_variable complete; ///< Task complete notification
};
