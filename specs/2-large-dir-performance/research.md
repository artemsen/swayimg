# Research: Large Directory Performance

## R1: FsMonitor Per-File vs Per-Directory Watches

**Decision:** Remove per-file `FsMonitor::add()` calls from `add_file()`, keep only per-directory calls in `add_dir()`.

**Rationale:** `FsMonitor::add()` calls `inotify_add_watch()` on the exact path given. Currently `add_file()` (imagelist.cpp:507) adds a watch for every individual file, while `add_dir()` (imagelist.cpp:476) already watches the parent directory. Directory-level watches with `IN_CREATE | IN_DELETE | IN_MOVE | IN_CLOSE_WRITE` already detect all file changes within the directory, making per-file watches redundant. For 50K files, this eliminates ~50K unnecessary inotify watches (Linux default limit is 8192 per user via `fs.inotify.max_user_watches`).

**Alternatives considered:**
- Keep per-file watches: rejected due to inotify limit exhaustion and overhead
- Use fanotify instead: rejected, more complex, overkill for this use case

## R2: Duplicate Detection Data Structure

**Decision:** Add `std::unordered_set<std::string>` as a path index alongside the existing `std::list<ImageEntryPtr>`.

**Rationale:** Current `add_entry()` does `O(n)` linear search via `std::find_if` over the full list. For 50K inserts this is `O(n²)`. An unordered_set on the path string provides `O(1)` amortized lookup. Memory overhead is ~50 bytes per entry (hash + string pointer + bucket overhead), acceptable per NFR1.

**Alternatives considered:**
- `std::set<std::string>`: O(log n) lookup, but unnecessary ordering overhead
- Hash on `std::filesystem::path` directly: no standard hash specialization, would need custom hasher
- `std::unordered_map<string, iterator>`: more complex, not needed since we only check existence

## R3: Async Scanning Architecture

**Decision:** Use a dedicated `std::thread` for background scanning (not ThreadPool) with batch-insert pattern.

**Rationale:** The scan is a single long-running task that should not occupy a ThreadPool slot for its entire duration. A dedicated thread scans directories and accumulates entries in a local buffer, then periodically flushes batches to the shared ImageList under a brief lock. This minimizes lock contention with the UI thread.

**Pattern:**
1. `load()` synchronously scans the first directory level to find the first image entry
2. Returns that entry immediately so the UI can start
3. Spawns a background thread to continue recursive scanning
4. Background thread accumulates entries in a local vector, flushes every ~100 entries
5. Each flush: lock, insert batch, unlock, send redraw event
6. On completion: lock, sort, reindex, unlock, send redraw event

**Alternatives considered:**
- ThreadPool tasks per directory: too many small tasks, overhead of task scheduling exceeds benefit
- Single ThreadPool task: occupies a thread for the full duration, blocking other ThreadPool users (gallery thumbnail loading)
- Coroutines: C++20 coroutines not widely supported/tested in this codebase

## R4: Sort-on-Completion Strategy

**Decision:** Sort the entire list once when scanning completes, in the scanner thread, with atomic swap.

**Rationale:** Incremental sorting (sort each batch) would require maintaining sorted invariant during concurrent insertion, adding complexity. Since the clarification confirmed discovery order during scan is acceptable, we sort once at the end. The sort runs in the scanner thread. To avoid blocking UI reads during sort, we can sort a copy and atomically swap, or hold the unique lock briefly (for 100K entries, `std::list::sort` takes ~50ms which exceeds the 16ms frame budget). Atomic swap approach: sort into a new list, then swap under lock.

**Alternatives considered:**
- Incremental sort (per-batch): adds complexity, not needed per clarification
- Sort in UI thread on completion event: blocks UI
- Parallel sort: C++17 parallel algorithms require random-access iterators, `std::list` doesn't support this

## R5: Scan State and Event Integration

**Decision:** Add a `scanning` atomic flag to ImageList and a new `AppEvent::ScanProgress` event type.

**Rationale:** The UI needs to know scanning is in progress (for FR6 progress indication) and when it completes (to update display). A simple `std::atomic<bool> scanning` flag lets the text system show a scanning indicator. The `ScanProgress` event triggers redraw to update the count display. On completion, a `ScanComplete` event triggers final sort and redraw.

**Alternatives considered:**
- Polling from UI thread: wasteful, breaks event-driven architecture
- Callback-based: more coupling between ImageList and Application
- Condition variable: blocks a thread, not suitable for poll()-based event loop

## R6: Gallery Mode Integration

**Decision:** Gallery already handles dynamic list changes via its existing redraw/reload mechanism. Background scan batches trigger `Application::redraw()` which causes Gallery to re-render with updated entries.

**Rationale:** Gallery's `load_thumbnails()` iterates visible entries and loads thumbnails via ThreadPool. When new entries appear during scanning, the next redraw cycle picks them up naturally. No special Gallery changes needed beyond ensuring it handles a growing list gracefully.

**Alternatives considered:**
- Gallery-specific scan notification: unnecessary, redraw is sufficient
- Pre-scan gallery layout: would show empty placeholders, confusing UX
