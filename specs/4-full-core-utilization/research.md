# Research: Full CPU Core Utilization

## Decision 1: Gallery Thumbnail Loading Strategy

**Decision**: Decouple thumbnail loading from the render path. Instead of calling `load_thumbnails()` from `window_redraw()`, trigger it from navigation events (`select()`, `activate()`, `handle_imagelist()`). The render path only reads from the cache.

**Rationale**: Currently `load_thumbnails()` is called every frame, acquires mutex, cancels all tasks, and re-queues. This creates a start-stop pattern that prevents sustained worker utilization. By moving loading triggers to navigation events, workers can run uninterrupted between navigations.

**Alternatives considered**:
- Timer-based loading (rejected: adds latency, unnecessary complexity)
- Separate loading thread with its own event loop (rejected: ThreadPool already exists)

## Decision 2: Eliminate tpool.cancel() on Every Frame

**Decision**: Replace the cancel-and-requeue pattern with incremental task management. Keep track of which entries are queued/active. On navigation, only add newly-visible entries to the queue. Never cancel tasks for entries that are still relevant (visible or within preload range).

**Rationale**: `tpool.cancel()` blocks until active tasks finish, then discards their results. This is the primary cause of worker thread stalls. Incremental management lets workers finish their tasks and have results cached.

**Alternatives considered**:
- Priority queue (rejected: doesn't solve the cancellation problem)
- Separate thread pool for gallery (rejected: already has its own `tpool`)

## Decision 3: Remove Mutex from draw() Path

**Decision**: The `draw()` function should not acquire the gallery mutex. Instead, make the thumbnail cache readable without locking during render by using a copy-on-read pattern or by ensuring cache modifications only happen on the main thread.

**Rationale**: `draw()` is called 20+ times per frame, each time acquiring the mutex. Since `load_thumbnail()` (worker) only appends to cache (push_back) and `draw()` only reads (find_if), we can use a separate mutex for cache reads vs. writes, or ensure the render path snapshot is lock-free.

**Implementation approach**:
- Worker threads put completed thumbnails into a separate `completed` queue (protected by a lightweight mutex)
- Main thread drains `completed` into `cache` before rendering (single point of synchronization)
- `draw()` reads `cache` without any lock (only main thread modifies it)
- `get_thumbnail()` becomes lock-free

## Decision 4: Lock Granularity for Gallery State

**Decision**: Split the single `mutex` into:
1. No mutex for `cache` (only accessed from main thread after drain)
2. A lightweight `completed_mutex` for the completed-thumbnails queue (workers push, main thread drains)
3. `queue` and `active` sets managed without locks by restructuring the flow

**Rationale**: The current single mutex protects cache, queue, and active sets. By ensuring cache is only modified on the main thread, and using a separate small mutex for the worker→main handoff, lock contention is eliminated.

## Decision 5: Render-Path Snapshot

**Decision**: Before rendering, the main thread drains completed thumbnails from workers into the cache. This is a single, fast operation. Then rendering proceeds without any locking.

**Rationale**: This is the producer-consumer pattern proven effective in the ImageList scanning refactor. Workers produce thumbnails into a thread-safe queue, main thread consumes them at defined sync points.
