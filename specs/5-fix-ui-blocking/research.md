# Research: Fix UI Blocking During Image Loading

**Created**: 2026-03-09

## R1: ThreadPool Cancellation Capabilities

**Decision**: ThreadPool's `cancel()` is already non-blocking (clears queue only, running tasks continue). The blocking methods are `wait()` and `wait(tid)`. No per-task cancellation token exists — app-level atomic flags (e.g., `image_pool.stop`) are used instead.

**Rationale**: `cancel()` returns immediately after clearing the task queue, which is sufficient for FR7 (non-blocking cancellation). The problem is that `Gallery::deactivate()` and `Viewer::preloader_stop()` call `tpool.wait()` after `cancel()`, which blocks until running tasks finish. The fix is to remove `wait()` calls from the main thread path and let tasks complete in the background or check stop flags.

**Alternatives considered**:
- Adding per-task cancellation tokens to ThreadPool — too invasive, app-level flags work well enough
- Making `wait()` interruptible — adds complexity without solving the root cause

## R2: Viewer Synchronous Load Architecture

**Decision**: `Viewer::open_file()` loads images synchronously on the main thread when preload cache misses. The fix is to dispatch the load to the existing `image_pool.tpool` and return immediately, keeping the previous image displayed.

**Rationale**: The preloader already uses `image_pool.tpool` for async loads with the same pattern (load → put in cache → redraw). The synchronous path in `open_file()` is a fallback for cache misses. Converting it to async reuses the existing infrastructure.

**Key implementation details**:
- Set `image_pool.stop = true` to cancel previous in-flight loads before dispatching new one
- Call `tpool.cancel()` to clear queued preloads
- Dispatch new load task, on completion: set image, call `on_open()`, trigger redraw
- The loop that retries on load failure must move into the async task
- Need atomic or mutex-protected "pending target" entry to support rapid navigation cancellation

**Alternatives considered**:
- Separate dedicated loading thread — unnecessary, ThreadPool already exists
- std::async/future — less control over cancellation than ThreadPool + atomic flag

## R3: Gallery Mutex Contention

**Decision**: Split gallery's single `mutex` into two: a `std::shared_mutex` for the thumbnail cache (read-heavy from render path) and a regular `std::mutex` for task coordination (queue/active sets). Worker threads acquire exclusive cache lock only when inserting completed thumbnails.

**Rationale**: The render path calls `get_thumbnail()` via `draw()` for every visible thumbnail (20+ per frame). Each call acquires the same mutex that workers hold during heavy I/O. With `shared_mutex`, multiple render reads proceed without blocking, and workers only briefly hold the exclusive lock when inserting a result.

**Alternatives considered**:
- Lock-free concurrent cache — complex to implement correctly, shared_mutex sufficient
- Copy-on-write cache snapshot — memory overhead for large thumbnail sets
- Per-entry locks — too fine-grained, cache iteration still needs coordination

## R4: Frame Callback Blocking

**Decision**: Replace blocking `frame_mutex.lock()` in `lock_surface()` with `try_lock()`. If frame is not ready, return a null pixmap and skip rendering this cycle. The event loop continues processing other events.

**Rationale**: Currently `lock_surface()` blocks indefinitely waiting for the compositor's frame callback. This prevents the event loop from processing keyboard/mouse events during the ~16ms wait. With `try_lock()`, the main thread checks if a frame is available, renders if so, and continues processing events if not. The next `poll()` iteration will check again.

**Key implementation details**:
- `lock_surface()` returns null pixmap if frame not ready
- `handle_event(WindowRedraw)` already checks `if (wnd)` — null pixmap naturally skips render
- A pending redraw event must be re-queued when frame is skipped, so rendering happens on next frame callback
- Frame callback should signal readiness via an eventfd monitored by `poll()`

**Alternatives considered**:
- Separate render thread — breaks single-threaded Wayland protocol assumption
- Timeout on frame_mutex — still blocks, just shorter
- Double-buffering with swap — Wayland already manages buffer lifecycle

## R5: Decoupling Thumbnail Loading from Render Path

**Decision**: Move `load_thumbnails()` call from `window_redraw()` to scroll/navigation event handlers. The render path only reads from the cache.

**Rationale**: Currently every frame triggers thumbnail task cancellation and re-queuing. This is wasteful and creates contention. Thumbnail loading should be triggered by user actions that change visible content (scroll, resize, mode entry), not by every render frame.

**Key implementation details**:
- Add `schedule_thumbnail_load()` called from key/mouse event handlers that affect gallery viewport
- `window_redraw()` only calls `draw()` and `clear_thumbnails()`
- Already-queued tasks for still-visible thumbnails continue executing (no cancel-on-scroll)
- Only cancel tasks when thumbnails scroll completely out of the extended viewport

**Alternatives considered**:
- Timer-based batching — adds latency without benefit
- Render-triggered but throttled — still couples render to load scheduling

## R6: Non-Blocking Mode Transitions

**Decision**: Replace `tpool.wait()` in `Gallery::deactivate()` and `Viewer::preloader_stop()` with `tpool.cancel()` only (non-blocking). Let running tasks complete in background with stop flags.

**Rationale**: Running tasks already check `image_pool.stop` flag. Gallery tasks can check a similar `Gallery::stopping` atomic. When the flag is set, tasks return early without updating cache. The cache can be cleared after mode transition without waiting for tasks.

**Key implementation details**:
- Set stop flag before `cancel()` so running tasks abort early
- Tasks must check stop flag before writing to cache
- Cache clear can happen on next `activate()` call
- Destructor still calls `wait()` for clean shutdown (acceptable — app is exiting)

**Alternatives considered**:
- Detached tasks — risk of use-after-free if Gallery is destroyed
- Reference counting on Gallery — overly complex for this use case
