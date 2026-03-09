# Implementation Plan: Fix UI Blocking During Image Loading

**Version**: 1.0
**Status**: Draft
**Created**: 2026-03-09
**Branch**: 5-fix-ui-blocking

## Technical Context

- **Language**: C++20
- **Build system**: Meson (>=1.1)
- **Threading**: std::thread, std::mutex, std::shared_mutex, std::atomic, ThreadPool
- **Event loop**: poll()-based, single main thread
- **Display**: Wayland (wl_shm buffers, frame callback sync)
- **Image decoding**: External libs (libjpeg, libpng, libwebp, etc.) via ImageLoader

## Constitution Check

No constitution file exists. Proceeding without governance constraints.

## Design Decisions

| ID | Decision | Rationale | Reference |
|----|----------|-----------|-----------|
| D1 | Async viewer load via existing ThreadPool | Reuses infrastructure, preloader already async | research.md R2 |
| D2 | Split gallery mutex into shared_mutex + mutex | Eliminates render/worker contention | research.md R3 |
| D3 | try_lock for frame sync | Non-blocking, skips render when frame not ready | research.md R4 |
| D4 | Move thumbnail loading to scroll events | Decouples render from I/O scheduling | research.md R5 |
| D5 | Remove tpool.wait() from main thread paths | Non-blocking mode transitions | research.md R6 |

## Phase 1: Non-Blocking Frame Sync (FR4)

**Goal**: Event loop never blocks waiting for compositor frame callback.

### T001: Make lock_surface() non-blocking

**File**: `src/ui_wayland.cpp`

Change `lock_surface()` to use `try_lock()` on `frame_mutex`:
- If lock acquired: also lock `wnd_buffer.mutex`, return pixmap (current behavior)
- If lock not acquired: return null/empty pixmap
- `handle_event(WindowRedraw)` already checks `if (wnd)` — null pixmap skips render naturally

**File**: `src/application.cpp`

When `lock_surface()` returns null pixmap (frame not ready), re-queue a `WindowRedraw` event so rendering happens on next frame callback. Add guard to prevent infinite re-queue loop (only re-queue if not already pending).

**Acceptance**: Main thread never blocks in `lock_surface()`. Events processed between frames.

### T002: Signal frame readiness via eventfd

**File**: `src/ui_wayland.cpp`

Add an eventfd that the frame callback signals when a frame is available. Register this fd in the poll set so the event loop wakes up when a frame is ready, rather than busy-polling.

- Frame callback: `eventfd_write(frame_ready_fd, 1)`
- Poll handler for frame_ready_fd: `eventfd_read()` → queue `WindowRedraw` event
- Remove the need for `frame_mutex` entirely if feasible, or keep as fast-path optimization

**Acceptance**: Render only triggered when compositor signals frame readiness. No busy-wait.

## Phase 2: Non-Blocking Gallery (FR2, FR3, FR6, FR7)

**Goal**: Gallery render path never blocks on thumbnail workers. Mode transitions are instant.

### T003: Split gallery mutex

**File**: `src/gallery.hpp`, `src/gallery.cpp`

Replace single `std::mutex mutex` with:
- `std::shared_mutex cache_mutex` — protects `cache` deque
- `std::mutex task_mutex` — protects `queue` and `active` sets

Update all access sites:
- `draw()`: acquire `cache_mutex` as shared lock (`std::shared_lock`)
- `get_thumbnail()`: called under shared lock (no own lock)
- `load_thumbnail()` worker: acquire `task_mutex` for queue/active updates, acquire `cache_mutex` exclusively only for `cache.emplace_back()`
- `load_thumbnails()`: acquire `task_mutex` for queue management
- `clear_thumbnails()`: acquire `cache_mutex` exclusively for cache cleanup

**Lock ordering**: `task_mutex` → `cache_mutex` (if both needed in same scope)

**Acceptance**: `draw()` never blocks on worker threads. No deadlocks.

### T004: Add stopping flag for gallery workers

**File**: `src/gallery.hpp`, `src/gallery.cpp`

Add `std::atomic<bool> stopping{false}` to Gallery.

- `deactivate()`: set `stopping = true`, call `tpool.cancel()` (non-blocking), do NOT call `tpool.wait()`
- `activate()`: set `stopping = false`, clear stale cache entries if needed
- `load_thumbnail()` worker: check `stopping` before `cache.emplace_back()` — if stopping, discard result and return
- Destructor: set `stopping = true`, `tpool.cancel()`, `tpool.wait()` (blocking OK at shutdown)

**Acceptance**: Mode switch from gallery to viewer completes in <10ms. No `wait()` on main thread.

### T005: Decouple thumbnail loading from render path

**File**: `src/gallery.cpp`

Move `load_thumbnails()` call from `window_redraw()` to navigation/scroll event handlers:
- Gallery key event handler (arrow keys, page up/down)
- Gallery mouse/scroll event handler
- Gallery `activate()` (initial load when entering gallery mode)
- Gallery resize handler

In `load_thumbnails()`:
- Remove `tpool.cancel()` call — don't cancel tasks for still-visible thumbnails
- Only queue tasks for thumbnails not already in cache, queue, or active set
- Add a separate method `cancel_offscreen_tasks()` that cancels tasks for thumbnails no longer in the extended viewport

`window_redraw()` becomes render-only: layout update, draw thumbnails from cache, clear old entries.

**Acceptance**: `window_redraw()` does no I/O scheduling. Thumbnails load progressively without cancel/re-queue churn.

## Phase 3: Async Viewer Image Loading (FR1, FR5)

**Goal**: Image navigation never blocks the main thread.

### T006: Async open_file() implementation

**File**: `src/viewer.hpp`, `src/viewer.cpp`

Add state for async loading:
- `std::atomic<bool> async_loading{false}` — whether a load is in progress
- `ImageEntryPtr pending_entry` — target entry for current async load (protected by `image_pool.mutex`)

Modify `open_file()`:
1. Check preload/history cache (existing fast path — keep)
2. On cache miss: instead of synchronous `ImageLoader::load()`:
   - Set `image_pool.stop = true` (cancel previous preloads)
   - Call `image_pool.tpool.cancel()` (clear queue)
   - Set `pending_entry` to target, `async_loading = true`
   - Set `image_pool.stop = false`
   - Dispatch async task to `image_pool.tpool`:
     ```
     task: load(entry) → if stop, return
           if fail, remove entry, try next (loop within task)
           if success, set image, call on_open(), redraw
     ```
3. Return immediately (previous image stays displayed)

Handle rapid navigation:
- If `async_loading` is true when `open_file()` called again:
  - Set `image_pool.stop = true` to signal current load to abort
  - Cancel queue, dispatch new load for latest target
  - The in-flight task checks `stop` flag and returns early

**Acceptance**: `open_file()` returns in <1ms. Image appears when ready. Rapid navigation cancels intermediate loads.

### T007: Non-blocking preloader_stop()

**File**: `src/viewer.cpp`

Remove `tpool.wait()` from `preloader_stop()`:
- Set `image_pool.stop = true`
- Call `tpool.cancel()` (already non-blocking)
- Do NOT call `tpool.wait()`
- Running tasks check `stop` flag and return early

Ensure tasks don't write to cache after `stop` is set:
- Check `stop` after `ImageLoader::load()` and before `preload.put()`
- Already implemented in current preloader tasks

**Acceptance**: `preloader_stop()` returns in <1ms. No blocking on task completion.

### T008: Thread-safe image state updates from async loads

**File**: `src/viewer.cpp`

When async load completes on worker thread:
- Acquire `image_pool.mutex`
- Check `stop` flag (may have been cancelled)
- Check `pending_entry` matches (may have been superseded)
- If valid: set `image = loaded_image`, update `pending_entry = nullptr`, `async_loading = false`
- Call `on_open()` via `Application::add_event()` (not directly — must run on main thread)
- Trigger `Application::redraw()`

Add new event type `AppEvent::ImageReady` with the loaded image:
- Handler runs on main thread: sets viewer image, calls `on_open()`, redraws
- Ensures all UI state updates happen on main thread (no race conditions)

**Acceptance**: No data races on viewer state. Image updates happen atomically on main thread.

## Phase 4: Integration & Validation

### T009: Event priority for input responsiveness

**File**: `src/application.cpp`

Ensure input events (keyboard, mouse) are processed before redraw events in the event queue:
- Current `add_event()` already inserts before trailing redraw events
- Verify that keyboard/mouse events from Wayland FD are dispatched before application events
- The poll() loop processes FDs in order — ensure Wayland display FD is checked first

**Acceptance**: Input events processed within 32ms even when redraw events are queued.

### T010: Stress testing with large directories

Manual validation:
- Open gallery on 10,000+ image directory
- Hold down scroll key — UI must remain responsive (30+ FPS)
- Switch between viewer and gallery — instant transition
- Navigate rapidly in viewer — previous image shown, no freeze
- Open 50MP+ image — UI responsive during decode

**Acceptance**: All success criteria from spec met under stress conditions.

## Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Data race in async image update | Medium | High | Use event-based main-thread dispatch (T008) |
| Deadlock from new lock ordering | Low | High | Strict ordering: task_mutex → cache_mutex |
| Stale image shown too long | Low | Medium | Preloader reduces cache misses; async load is fast |
| Frame skipping causes visual stutter | Medium | Medium | eventfd-based frame signaling (T002) avoids busy-wait |
| Background tasks outlive Gallery/Viewer | Medium | High | Stop flags + cancel before destruction |

## Dependency Graph

```
T001 (non-blocking lock_surface)
  └── T002 (frame readiness eventfd)

T003 (split gallery mutex)
  ├── T004 (stopping flag)
  └── T005 (decouple thumbnail from render)

T006 (async open_file)
  ├── T007 (non-blocking preloader_stop)
  └── T008 (thread-safe image updates)

T009 (event priority) — independent

T010 (stress testing) — depends on all above
```

## Implementation Order

1. **T001 + T002** — Non-blocking frame sync (foundation for all other changes)
2. **T003** — Split gallery mutex (enables T004, T005)
3. **T004** — Gallery stopping flag (enables non-blocking deactivate)
4. **T005** — Decouple thumbnail loading from render
5. **T006** — Async viewer loading (core feature)
6. **T007** — Non-blocking preloader_stop
7. **T008** — Thread-safe async image updates
8. **T009** — Event priority validation
9. **T010** — Integration stress testing
