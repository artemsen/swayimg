// SPDX-License-Identifier: MIT
// Thread pool.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "tpool.h"

#include "list.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef __FreeBSD__
#include <sys/sysctl.h>
#endif

// Max number of threads in the pool
#define MAX_THREADS 16

struct tpool_task {
    struct list list; ///< Links to prev/next entry
    tpool_worker wfn; ///< Task handler
    tpool_free ffn;   ///< Task-specific data deleter
    void* data;       ///< Task-specific data
};

/** Context of the thread pool. */
struct tpool {
    size_t size;    ///< Number of threads in the pool
    size_t work;    ///< Number of currently worked threads (not waiting)
    pthread_t* ids; ///< Array of thead ids
    struct tpool_task* queue; ///< Task queue
    pthread_cond_t qtask;     ///< Task queue notification
    pthread_cond_t idle;      ///< All threads in idle mode
    pthread_mutex_t lock;     ///< Lock
};

/** Global thread pool instance. */
static struct tpool ctx;

/** Working thread. */
static void* worker(__attribute__((unused)) void* data)
{
    bool alive = true;
    while (alive) {
        struct tpool_task* task;

        pthread_mutex_lock(&ctx.lock);
        while (!ctx.queue) {
            pthread_cond_wait(&ctx.qtask, &ctx.lock);
        }
        task = ctx.queue;
        ctx.queue = list_remove(task);
        ++ctx.work;
        pthread_mutex_unlock(&ctx.lock);

        if (task->wfn) {
            task->wfn(task->data);
        } else {
            alive = false;
        }

        if (task->ffn) {
            task->ffn(task->data);
        }
        free(task);

        pthread_mutex_lock(&ctx.lock);
        if (--ctx.work == 0) {
            pthread_cond_signal(&ctx.idle);
        }
        pthread_mutex_unlock(&ctx.lock);
    }

    return NULL;
}

void tpool_init(void)
{
    // get number of active CPUs
    int32_t cpus = 0;
#ifdef __FreeBSD__
    size_t cpus_len = sizeof(cpus);
    sysctlbyname("hw.ncpu", &cpus, &cpus_len, NULL, 0);
#else
    cpus = sysconf(_SC_NPROCESSORS_ONLN);
#endif
    --cpus; // preserve one thread as "main"
    if (cpus <= 0) {
        ctx.size = 1;
    } else if (cpus > MAX_THREADS) {
        ctx.size = MAX_THREADS;
    } else {
        ctx.size = cpus;
    }

    ctx.ids = malloc(ctx.size * sizeof(*ctx.ids));
    if (!ctx.ids) {
        ctx.size = 0;
        return;
    }

    pthread_cond_init(&ctx.idle, NULL);
    pthread_cond_init(&ctx.qtask, NULL);
    pthread_mutex_init(&ctx.lock, NULL);

    for (size_t i = 0; i < ctx.size; ++i) {
        if (pthread_create(&ctx.ids[i], NULL, worker, NULL) != 0) {
            ctx.size = i;
            return;
        }
    }
}

void tpool_destroy(void)
{
    // send exit notification
    for (size_t i = 0; i < ctx.size; ++i) {
        tpool_add_task(NULL, NULL, NULL);
    }
    // wait for threads exit
    for (size_t i = 0; i < ctx.size; ++i) {
        pthread_join(ctx.ids[i], NULL);
    }

    pthread_mutex_destroy(&ctx.lock);
    pthread_cond_destroy(&ctx.qtask);
    pthread_cond_destroy(&ctx.idle);
    free(ctx.ids);
}

size_t tpool_threads(void)
{
    return ctx.size;
}

void tpool_add_task(tpool_worker wfn, tpool_free ffn, void* data)
{
    struct tpool_task* task = malloc(sizeof(struct tpool_task));
    if (task) {
        task->wfn = wfn;
        task->ffn = ffn;
        task->data = data;
        pthread_mutex_lock(&ctx.lock);
        ctx.queue = list_append(ctx.queue, task);
        pthread_cond_broadcast(&ctx.qtask);
        pthread_mutex_unlock(&ctx.lock);
    }
}

void tpool_cancel(void)
{
    if (ctx.queue) {
        pthread_mutex_lock(&ctx.lock);
        list_for_each(ctx.queue, struct tpool_task, it) {
            if (it->ffn) {
                it->ffn(it->data);
            }
            free(it);
        }
        ctx.queue = NULL;
        pthread_mutex_unlock(&ctx.lock);
    }
}

void tpool_wait(void)
{
    pthread_mutex_lock(&ctx.lock);
    if (ctx.work || ctx.queue) {
        pthread_cond_wait(&ctx.idle, &ctx.lock);
    }
    pthread_mutex_unlock(&ctx.lock);
}
