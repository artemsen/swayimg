# Research: Parallel Startup and Core Utilization

## Decision 1: Parallel Scan Architecture

**Decision**: Use existing ThreadPool to dispatch per-directory scan tasks. Each task scans one directory (non-recursively) and enqueues child directories as new tasks. This creates a natural work-distribution pattern without explicit work-stealing.

**Rationale**: The ThreadPool already supports tasks adding new tasks (`add()` from within a task executor is safe). Each directory becomes one task — the pool's FIFO queue naturally distributes work across idle threads. Deep subtrees automatically generate more tasks that other threads pick up, achieving work-stealing-like behavior without additional infrastructure.

**Alternatives considered**:
- Dedicated scan thread pool (rejected: unnecessary complexity, ThreadPool already exists)
- Fork-join with `std::async` (rejected: no control over thread count, no cancellation)
- Work-stealing deque per thread (rejected: over-engineered for filesystem I/O)

## Decision 2: Batch Insertion Strategy

**Decision**: Use per-task batch buffers with a shared concurrent insertion queue. Each scan task accumulates entries locally, then pushes the batch to a thread-safe queue. A periodic flush from the main event loop (or a dedicated coalescing mechanism) drains the queue under a single lock acquisition.

**Rationale**: Current `flush_batch()` acquires exclusive lock + calls `reindex()` (O(n)) every 100 entries. With 8 threads scanning simultaneously, lock contention multiplies. Moving to a producer-consumer pattern where scan tasks push to a lock-free or low-contention queue, and a single consumer drains it periodically, eliminates contention between scan threads.

**Alternatives considered**:
- Lock-free queue (rejected: `std::list` insertion still needs synchronization; complexity not justified)
- Fine-grained locking on list segments (rejected: `std::list` doesn't support this)
- Double-buffering (considered: viable but producer-consumer is simpler)

## Decision 3: Deferred Reindex

**Decision**: Remove per-batch `reindex()` calls during scanning. Only reindex once in `finish_scan()` after all entries are inserted. During scanning, entry indices are stale but this is acceptable since navigation uses iterators (not indices) and the display shows "scanning..." state.

**Rationale**: `reindex()` iterates the entire list (O(n)), called every 100 entries. For 40,000 files, that's 400 reindex calls with cumulative cost O(n²/100) ≈ O(n²). Eliminating intermediate reindex is a major performance win. The entry `index` field is only used for display ("Image 5 of 1000") — showing a stale or zero index during scanning is acceptable.

**Alternatives considered**:
- Atomic counter for display (considered: simpler but doesn't fix the reindex cost)
- Incremental index update (rejected: still O(batch_size) per flush under lock)

## Decision 4: First-Image Fast Path

**Decision**: Submit all top-level directories as scan tasks immediately. The first task that finds an image file signals the main thread via an event. The main thread opens that image and starts the viewer while remaining tasks continue scanning.

**Rationale**: Current code synchronously scans the first directory level and even descends deeper if no files are found. With parallel tasks, all directories start scanning simultaneously — the first image is found by whichever thread reaches an image file first, regardless of directory depth.

**Alternatives considered**:
- Priority queue for shallow directories (rejected: filesystem layout is unpredictable)
- Synchronous BFS for first file only (rejected: still serial, parallel is faster)

## Decision 5: Scan Coordination

**Decision**: Use an `std::atomic<size_t>` active task counter. Each scan task increments on start, decrements on finish. When counter reaches 0, call `finish_scan()`. The `scanning` atomic bool remains for cancellation signaling.

**Rationale**: With N parallel tasks dynamically spawning child tasks, a simple counter is the cleanest way to know when all work is done. No need for `wait()` on the ThreadPool since preloader tasks may also be queued.

**Alternatives considered**:
- ThreadPool::wait() (rejected: would also wait for preloader tasks)
- Completion callback per task (rejected: complex, error-prone)
- Separate task ID tracking (rejected: unnecessary overhead)

## Decision 6: Shared ThreadPool Between Scanner and Preloader

**Decision**: Scanner uses `Viewer::image_pool.tpool` (the same ThreadPool the preloader uses). During startup, scanning tasks fill the pool. Once scanning nears completion and the user views images, preloader tasks are also queued. Both coexist naturally.

**Rationale**: The spec clarification confirmed reusing the existing ThreadPool. Since scanning is I/O-heavy and preloading is decode-heavy, they can share threads efficiently. The pool has up to 8 threads — enough for both workloads.

**Issue**: The ThreadPool is owned by `Viewer::image_pool`. ImageList needs access to it. Options:
- Pass ThreadPool reference to ImageList::load()
- Make ImageList use its own ThreadPool
- Use the Application-level ThreadPool if one exists

**Resolution**: ImageList should have its own ThreadPool member for scanning. This is cleaner than coupling ImageList to Viewer. The preloader's ThreadPool in Viewer remains separate. Both pools draw from hardware threads — the OS scheduler handles core assignment. Total thread count: up to 16 (8 scan + 8 preload) but in practice they don't run heavy workloads simultaneously.

**Updated decision**: Give ImageList its own ThreadPool for scanning, replacing the single `scan_thread`. This avoids architectural coupling between ImageList and Viewer.
