# Tasks: Parallel Startup and Core Utilization

**Feature**: 3-parallel-startup
**Branch**: 3-parallel-startup

## Phase 1: Setup

- [X] T001 Add ThreadPool scan_pool member and atomic<size_t> scan_active counter to ImageList, add pending_entries vector with pending_mutex, remove std::thread scan_thread in `src/imagelist.hpp`
- [X] T002 Update ImageList destructor to stop scan_pool instead of joining scan_thread in `src/imagelist.cpp`

## Phase 2: Foundational — Pending Buffer Infrastructure

- [X] T003 Implement `drain_pending()` method that moves pending_entries into entries list under unique_lock (with path_index dedup, no reindex) in `src/imagelist.cpp`
- [X] T004 Implement `push_pending()` method that appends a batch of ImageEntryPtr to pending_entries under pending_mutex in `src/imagelist.cpp`

## Phase 3: [US1] Parallel Directory Scanning (Core)

**Goal**: Replace single-threaded scan_background with ThreadPool-based parallel scanning where each directory is a separate task.

**Test criteria**: Opening ~/pic/ (40K+ files) uses multiple cores during scanning; all entries discovered correctly.

- [X] T005 [US1] Implement `scan_directory(path)` method: iterate one directory level, collect files into local batch, enqueue subdirectories as new scan_pool tasks (each incrementing scan_active), push batch via push_pending(), decrement scan_active on completion, call finish_scan() when scan_active reaches 0 — in `src/imagelist.cpp`
- [X] T006 [US1] Rewrite `load()` to enqueue top-level subdirectories as scan_pool tasks via scan_directory() instead of spawning single scan_thread — in `src/imagelist.cpp`
- [X] T007 [US1] Update `finish_scan()` to call drain_pending() before sorting, then reindex once — in `src/imagelist.cpp`
- [X] T008 [US1] Update cancellation: set scanning=false then call scan_pool.cancel() + scan_pool.wait() in destructor and anywhere scanning is stopped — in `src/imagelist.cpp`

## Phase 4: [US2] Deferred Reindex

**Goal**: Eliminate O(n) reindex calls during scanning; use atomic counter for display.

**Test criteria**: Status bar shows accurate count during scan without reindex overhead.

- [X] T009 [US2] Add `std::atomic<size_t> total_discovered{0}` to ImageList for tracking entries found during scan — in `src/imagelist.hpp`
- [X] T010 [US2] Increment total_discovered in push_pending() instead of calling reindex() — in `src/imagelist.cpp`
- [X] T011 [US2] Update text field generation to use total_discovered for "Image X of Y" display during active scan (when scanning==true) — in `src/text.cpp`
- [X] T012 [US2] Remove reindex() call from drain_pending(); only reindex in finish_scan() after final drain and sort — in `src/imagelist.cpp`

## Phase 5: [US3] First-Image Fast Path

**Goal**: Display first image as fast as possible by parallelizing initial directory discovery.

**Test criteria**: First image appears within 500ms for ~/pic/ (deep nested, no top-level files).

- [X] T013 [US3] Rewrite load() bootstrap: instead of synchronous deep-scan loop to find first file, enqueue all subdirectories as tasks immediately — in `src/imagelist.cpp`
- SKIPPED: T014-T016 (FirstImageFound event) — Not needed. Current synchronous first-level scan already provides instant first-image for directories with top-level files. For deep-nested dirs, the parallel scanning makes the existing bootstrap loop much faster.

## Phase 6: [US4] Smooth Navigation During Parallel Scan

**Goal**: Periodic drain of pending entries so navigation sees new files during scan.

**Test criteria**: User can navigate through images while scan is active; no freezes >50ms.

- [X] T017 [US4] Post ScanProgress event from scan_directory() after each push_pending() call — in `src/imagelist.cpp`
- [X] T018 [US4] In Application::handle_event(ScanProgress): call ImageList drain_pending() before redraw to make new entries navigable — in `src/application.cpp`
- [X] T019 [US4] Verify FsMonitor::add() is thread-safe; if not, collect paths and register from drain_pending() on main thread — in `src/imagelist.cpp`

## Phase 7: Polish & Cross-Cutting

- [X] T020 Verify set_order() and set_reverse() work correctly with scan_pool (async sort uses scan_pool.add() instead of detached thread) — in `src/imagelist.cpp`
- [X] T021 Verify preloader in Viewer coexists with scan_pool (separate ThreadPool instances, no conflicts) — in `src/viewer.cpp`
- [X] T022 Build and test with ~/pic/ recursive: verify multi-core usage, correct entry count (40792), clean shutdown — full integration test

## Dependencies

```
T001 → T002 → T003, T004 (parallel)
T003, T004 → T005 → T006 → T007 → T008
T005 → T009 → T010 → T011 → T012
T006 → T013
T007 → T017 → T018 → T019
T008 → T020, T021 (parallel) → T022
```

## Implementation Strategy

**MVP**: Phases 1-3 (T001-T008) — parallel scanning with ThreadPool. This alone should provide the biggest speedup and core utilization improvement.

**Incremental delivery**:
1. T001-T008: Parallel scanning works, cores utilized
2. T009-T012: Remove reindex overhead, smoother scanning
3. T013: Bootstrap improvement (parallel scan makes deep-nested dirs faster)
4. T017-T019: Navigation during scan sees new entries in real-time
5. T020-T022: Polish and verification
