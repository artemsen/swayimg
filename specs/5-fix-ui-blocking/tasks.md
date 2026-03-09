# Tasks: Fix UI Blocking During Image Loading

**Feature**: 5-fix-ui-blocking
**Created**: 2026-03-09
**Total Tasks**: 12

## Phase 1: Foundational — Non-Blocking Frame Sync

**Goal**: Event loop never blocks waiting for compositor frame callback.
**Addresses**: FR4 (Non-Blocking Frame Submission), FR5 (Responsive Input Processing)
**Test criteria**: Main thread returns from `lock_surface()` immediately regardless of frame callback state. Events processed between frames.

- [x] T001 Make `lock_surface()` non-blocking using `try_lock()` on `frame_mutex` in `src/ui_wayland.cpp`
- [x] T002 Re-queue `WindowRedraw` event when frame not ready, with dedup guard, in `src/application.cpp`
- [x] T003 Add frame-ready eventfd signaled by frame callback, register in poll set, in `src/ui_wayland.cpp`

## Phase 2: Non-Blocking Gallery — Mutex Split

**Goal**: Gallery render path never blocks on thumbnail workers.
**Addresses**: FR2 (Non-Blocking Gallery Thumbnail Access)
**Test criteria**: `draw()` acquires only shared lock, never blocks on worker threads. No deadlocks under concurrent access.

- [x] T004 Replace single `std::mutex mutex` with `std::shared_mutex cache_mutex` + `std::mutex task_mutex` in `src/gallery.hpp`
- [x] T005 Update `draw()` and `get_thumbnail()` to use `std::shared_lock<std::shared_mutex>` on `cache_mutex` in `src/gallery.cpp`
- [x] T006 Update `load_thumbnail()` worker to use `task_mutex` for queue/active, exclusive `cache_mutex` only for `cache.emplace_back()` in `src/gallery.cpp`
- [x] T007 Update `load_thumbnails()` and `clear_thumbnails()` to use correct mutex per data structure in `src/gallery.cpp`

## Phase 3: Non-Blocking Gallery — Mode Transitions & Decoupling

**Goal**: Mode transitions are instant. Thumbnail loading driven by user actions, not render frames.
**Addresses**: FR3 (Non-Blocking Mode Transitions), FR6 (Eliminate Render-Path Thumbnail Loading), FR7 (Non-Blocking Task Cancellation)
**Test criteria**: `deactivate()` returns in <10ms. `window_redraw()` performs no I/O scheduling. Thumbnails load progressively without cancel/re-queue churn.

- [x] T008 Add `std::atomic<bool> stopping` flag, update `deactivate()` to set flag + `cancel()` without `wait()`, update `activate()` to clear flag, in `src/gallery.hpp` and `src/gallery.cpp`
- [x] T009 Check `stopping` flag in `load_thumbnail()` worker before `cache.emplace_back()`, discard result if stopping, in `src/gallery.cpp`
- [x] T010 Move `load_thumbnails()` call from `window_redraw()` to key/mouse/scroll event handlers and `activate()` in `src/gallery.cpp`
- [x] T011 [P] Remove `tpool.cancel()` from `load_thumbnails()`, only queue tasks for uncached/unqueued/inactive thumbnails in `src/gallery.cpp`

## Phase 4: Async Viewer Image Loading

**Goal**: Image navigation never blocks the main thread. Previous image shown during async load. Rapid navigation cancels intermediate loads.
**Addresses**: FR1 (Asynchronous Image Loading in Viewer)
**Test criteria**: `open_file()` returns in <1ms on cache miss. Image appears when ready via event. Rapid key presses cancel intermediate loads and show only the final target.

- [x] T012 Add `AppEvent::ImageReady` event type carrying loaded image + entry in `src/application.hpp`
- [x] T013 Add `ImageReady` event handler in `src/application.cpp` that sets viewer image, calls `on_open()`, triggers redraw
- [x] T014 Add `async_loading` atomic and `pending_entry` field to `Viewer::ImagePool` in `src/viewer.hpp`
- [x] T015 Convert synchronous load loop in `open_file()` to async ThreadPool dispatch with stop-flag cancellation in `src/viewer.cpp`
- [x] T016 Move failure retry loop (remove entry, try next) into async task, dispatch `ImageReady` or `FileRemove` event on completion in `src/viewer.cpp`
- [x] T017 Remove `tpool.wait()` from `preloader_stop()`, rely on `stop` flag for running task cancellation in `src/viewer.cpp`

## Phase 5: Polish & Cross-Cutting Concerns

**Goal**: Verify input responsiveness and overall integration under stress.
**Addresses**: NFR1 (Input Latency), NFR2 (Frame Rate), NFR3 (Thread Safety)
**Test criteria**: All success criteria from spec met. No data races under ThreadSanitizer. Input processed within 32ms at all times.

- [x] T018 Verify input event priority: Wayland display FD processed before application events in poll loop in `src/application.cpp`
- [ ] T019 Manual stress test: gallery with 10,000+ images, rapid scrolling, mode switching, large image navigation

## Dependencies

```
T001 → T002 → T003  (frame sync chain)

T004 → T005, T006, T007  (mutex split enables all gallery lock updates)
T005, T006, T007 → T008  (lock updates before stopping flag)
T008 → T009  (stopping flag before worker check)
T008, T009 → T010, T011  (decouple after stopping is safe)

T012 → T013  (event type before handler)
T013 → T014 → T015 → T016  (async viewer chain)
T015 → T017  (async open_file before non-blocking preloader_stop)

T018 — independent (can run any time)
T019 — depends on all above
```

## Parallel Execution Opportunities

**Group A** (independent from Group B):
- T001–T003 (frame sync) can proceed in parallel with T004–T011 (gallery)

**Group B** (independent from Group A):
- T004–T007 (mutex split) can proceed in parallel with T001–T003 (frame sync)

**Group C** (after Groups A & B):
- T012–T017 (async viewer) — depends on understanding of event patterns from T001–T003

**Within Phase 2**:
- T005, T006, T007 are parallelizable after T004 (different functions, same mutex change)

**Within Phase 4**:
- T012 + T014 are parallelizable (event type + viewer state, different files)

## Implementation Strategy

**MVP (Minimum Viable)**: Phase 1 (T001–T003) + Phase 4 (T012–T017)
- Frame sync unblocking + async viewer loading addresses the most user-visible freezes
- Gallery improvements (Phase 2–3) can follow as incremental enhancement

**Full scope**: All 19 tasks across 5 phases

**Incremental delivery**:
1. After Phase 1: UI responds between frames, keyboard no longer freezes during render waits
2. After Phase 2–3: Gallery scrolling smooth, mode transitions instant
3. After Phase 4: Image navigation fully async, no freezes on cache miss
4. After Phase 5: Validated under stress, all success criteria confirmed
