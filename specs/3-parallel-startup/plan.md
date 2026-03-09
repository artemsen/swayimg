# Implementation Plan: Parallel Startup and Core Utilization

**Feature**: 3-parallel-startup
**Branch**: 3-parallel-startup
**Status**: Planning

## Technical Context

- **Language**: C++20
- **Build system**: Meson (>=1.1)
- **Threading**: std::thread, std::atomic, std::shared_mutex, ThreadPool (custom)
- **Key files to modify**:
  - `src/imagelist.hpp` — Add ThreadPool, atomic counter, pending buffer
  - `src/imagelist.cpp` — Rewrite scan_background/scan_recursive to use ThreadPool tasks
  - `src/viewer.cpp` — Minor: ensure preloader coexists with scan pool
- **Dependencies**: Existing ThreadPool class (no modifications needed)

## Architecture

### Current Flow (Single-Threaded Scan)
```
load() → sync scan level 1 → spawn single scan_thread
  scan_thread: scan_recursive(dir1) → scan_recursive(dir2) → ... → finish_scan()
  Each scan_recursive: DFS tree, batch 100 entries, flush_batch(lock + reindex)
```

### New Flow (Parallel Scan via ThreadPool)
```
load() → sync scan level 1 → enqueue all subdirs as ThreadPool tasks
  Task per directory:
    1. Iterate directory (non-recursive)
    2. Files → push to thread-local batch
    3. Subdirectories → enqueue as new tasks (scan_active++)
    4. Batch full (100) → push to shared pending_entries (lightweight mutex)
    5. scan_active-- when done
    6. Last task (scan_active → 0): drain pending, sort, reindex, ScanComplete
```

### Key Design Decisions

1. **One task per directory**: Each ThreadPool task scans a single directory level. Child directories become new tasks. This distributes work naturally across threads.

2. **Deferred reindex**: No `reindex()` during scanning. Entry indices are stale during scan (acceptable — "scanning..." indicator shown). Single reindex at completion.

3. **Pending buffer pattern**: Scan tasks push batches to `pending_entries` (protected by lightweight `pending_mutex`). Periodically drained into `entries` list by a drain task or on completion. This avoids contention on the main `shared_mutex`.

4. **Own ThreadPool**: ImageList gets its own ThreadPool member for scanning. Avoids coupling to Viewer's preloader pool. OS scheduler distributes both pools across cores.

5. **First-image fast path**: All top-level subdirectories enqueued simultaneously. First thread to find an image file triggers display. No synchronous deep-scan needed.

## File Structure

```
src/
├── imagelist.hpp      # Add: ThreadPool scan_pool, atomic<size_t> scan_active,
│                      #       vector<ImageEntryPtr> pending_entries, mutex pending_mutex
│                      # Remove: std::thread scan_thread, atomic<bool> sorting
│                      # Add: scan_directory() (single-dir task), drain_pending()
├── imagelist.cpp      # Rewrite: load() to enqueue tasks instead of spawning thread
│                      # Rewrite: scan_background() → removed (replaced by tasks)
│                      # Rewrite: scan_recursive() → scan_directory() (non-recursive)
│                      # Rewrite: flush_batch() → push to pending_entries
│                      # Add: drain_pending() — move pending into entries under lock
│                      # Modify: finish_scan() — drain + sort + reindex
├── threadpool.hpp     # Unchanged
├── threadpool.cpp     # Unchanged
├── viewer.cpp         # Minor: verify preloader doesn't conflict with scan pool
├── appevent.hpp       # Unchanged (ScanProgress/ScanComplete already exist)
└── application.cpp    # Minor: ScanProgress handler may call drain_pending()
```

## Implementation Phases

### Phase 1: Replace scan_thread with ThreadPool
- Add `ThreadPool scan_pool` to ImageList (replace `std::thread scan_thread`)
- Add `std::atomic<size_t> scan_active{0}` for task counting
- Update destructor to use `scan_pool.stop()` instead of thread join

### Phase 2: Implement per-directory task scanning
- New `scan_directory(path)` method: scans one directory, enqueues subdirs as tasks
- Each task: increment scan_active on start, decrement on finish
- When scan_active reaches 0: call finish_scan()
- Handle cancellation via `scanning` atomic flag

### Phase 3: Pending buffer for low-contention insertion
- Add `pending_entries` vector + `pending_mutex`
- Scan tasks push batches to pending_entries (not directly to entries list)
- `drain_pending()`: moves pending_entries into entries under shared_mutex unique_lock
- Call drain_pending() periodically (on ScanProgress) and at finish

### Phase 4: Deferred reindex
- Remove `reindex()` calls from batch insertion
- Add display-count tracking via `std::atomic<size_t> total_discovered`
- Text layer uses `total_discovered` for "Image X of Y" during scan
- Single reindex in finish_scan() after sort

### Phase 5: First-image optimization
- In load(): enqueue ALL subdirectories immediately (don't sync-scan first level for files)
- First scan task that finds a file posts FirstImageFound event
- Main thread opens that file immediately
- Remove synchronous bootstrap scan loop

## Risk Assessment

| Risk | Mitigation |
|------|-----------|
| Data race on entries list | pending_entries decouples producers from consumers |
| Deadlock between scan_pool and shared_mutex | Scan tasks never hold shared_mutex; drain_pending() is the only writer |
| Too many tasks for deeply nested dirs | ThreadPool queue is unbounded; tasks are lightweight |
| FsMonitor thread safety | FsMonitor::add() already called per-directory; verify it's thread-safe |
| scan_active counter miscount | Use RAII guard or careful increment/decrement at task boundaries |
