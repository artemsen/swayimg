# Tasks: Full CPU Core Utilization

**Feature**: 4-full-core-utilization
**Branch**: 4-full-core-utilization

## Phase 1: Setup — Completed Queue Infrastructure

- [X] T001 Add `std::vector<ThumbEntry> completed` and `std::mutex completed_mutex` members to Gallery, alongside existing cache/mutex in `src/gallery.hpp`
- [X] T002 Implement `drain_completed()` method: lock completed_mutex, move all entries from completed into cache, unlock — in `src/gallery.cpp`

## Phase 2: Foundational — Decouple Loading from Render

- [X] T003 In `window_redraw()`: add `drain_completed()` call before drawing thumbnails in `src/gallery.cpp`
- [X] T004 In `window_redraw()`: remove `load_thumbnails()` call (line 340) in `src/gallery.cpp`
- [X] T005 Add `load_thumbnails()` call to `activate()` method after layout setup in `src/gallery.cpp`
- [X] T006 Add `load_thumbnails()` call to `select()` method after layout navigation in `src/gallery.cpp`
- [X] T007 Add `load_thumbnails()` call to `handle_imagelist()` for Create/Remove events in `src/gallery.cpp`
- [X] T008 Add `load_thumbnails()` call after `drain_completed()` in `window_redraw()` to handle scroll-triggered visibility changes in `src/gallery.cpp`

## Phase 3: [US1] Rewrite Thumbnail Loading — No Cancel, Incremental

**Goal**: Workers run to completion without being cancelled every frame. Only new tasks queued for uncached entries.

**Test criteria**: Gallery thumbnails load progressively; no start-stop-start pattern visible in htop.

- [X] T009 [US1] Rewrite `load_thumbnails()`: remove `tpool.cancel()` and `queue.clear()` at the top in `src/gallery.cpp`
- [X] T010 [US1] In rewritten `load_thumbnails()`: call `drain_completed()` first to ensure cache is current in `src/gallery.cpp`
- [X] T011 [US1] In rewritten `load_thumbnails()`: for each visible entry, skip if found in cache (via get_thumbnail) or in completed queue (check under completed_mutex) in `src/gallery.cpp`
- [X] T012 [US1] In rewritten `load_thumbnails()`: only enqueue tpool.add() for entries not in cache and not already being processed in `src/gallery.cpp`

## Phase 4: [US2] Rewrite Worker — Push to Completed, Not Cache

**Goal**: Worker threads never touch the cache directly. They push decoded thumbnails to the completed queue.

**Test criteria**: Multiple worker threads decode thumbnails simultaneously without mutex contention.

- [X] T013 [US2] Rewrite `load_thumbnail()`: remove the initial `std::lock_guard lock(mutex)` block that inserts into active/removes from queue in `src/gallery.cpp`
- [X] T014 [US2] Rewrite `load_thumbnail()`: at the end, replace `std::lock_guard lock(mutex)` + cache push + active erase with `std::lock_guard lock(completed_mutex)` + completed push in `src/gallery.cpp`

## Phase 5: [US3] Remove Mutex from Render Path

**Goal**: draw() and clear_thumbnails() execute without any lock acquisition.

**Test criteria**: Gallery renders 20+ thumbnails per frame with zero mutex acquisitions in the draw loop.

- [X] T015 [US3] Remove `std::lock_guard lock(mutex)` from `draw()` method in `src/gallery.cpp`
- [X] T016 [US3] Remove `std::lock_guard lock(mutex)` from `clear_thumbnails()` method in `src/gallery.cpp`
- [X] T017 [US3] Remove `std::lock_guard lock(mutex)` from `load_thumbnails()` if still present in `src/gallery.cpp`
- [X] T018 [US3] Remove old `mutex`, `queue`, `active` members from Gallery class in `src/gallery.hpp`

## Phase 6: [US4] Dedup Tracking for Queued Tasks

**Goal**: Prevent duplicate task submission for entries already being decoded.

**Test criteria**: Same entry is never queued twice; no wasted decode work.

- [X] T019 [US4] Add `std::unordered_set<ImageEntryPtr> queued` member to Gallery protected by completed_mutex in `src/gallery.hpp`
- [X] T020 [US4] In `load_thumbnails()`: check queued set before adding task, insert entry into queued when task is added in `src/gallery.cpp`
- [X] T021 [US4] In `load_thumbnail()` (worker): under completed_mutex, erase entry from queued when pushing to completed in `src/gallery.cpp`
- [X] T022 [US4] In `drain_completed()`: clear drained entries from queued set in `src/gallery.cpp`

## Phase 7: Polish & Cross-Cutting

- [X] T023 Keep `tpool.cancel()` in `deactivate()` for clean mode-switch shutdown in `src/gallery.cpp`
- [X] T024 Verify Vulkan gallery path `window_redraw_vk()` also calls drain_completed() and removes per-thumbnail locking in `src/gallery.cpp`
- [X] T025 Build and test with large directory in gallery mode: verify multi-core usage, smooth scrolling, correct thumbnail display — full integration test

## Dependencies

```
T001 → T002
T002 → T003 → T004, T005, T006, T007, T008 (parallel)
T004, T008 → T009 → T010 → T011 → T012
T012 → T013 → T014
T014 → T015 → T016 → T017 → T018
T018 → T019 → T020 → T021 → T022
T022 → T023, T024 (parallel) → T025
```

## Parallel Execution Opportunities

- T004, T005, T006, T007, T008 can run in parallel (different methods, independent)
- T023 and T024 can run in parallel (different concerns)

## Implementation Strategy

**MVP**: Phases 1-4 (T001-T014) — completed queue + no-cancel loading + worker rewrite. This alone eliminates the main single-core bottleneck.

**Incremental delivery**:
1. T001-T002: Completed queue infrastructure
2. T003-T008: Decouple loading from render
3. T009-T012: Incremental loading without cancel
4. T013-T014: Workers push to completed (sustained parallelism achieved)
5. T015-T018: Remove all locks from render path
6. T019-T022: Dedup tracking
7. T023-T025: Polish and integration test
