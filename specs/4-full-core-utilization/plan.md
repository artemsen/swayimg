# Implementation Plan: Full CPU Core Utilization

**Feature**: 4-full-core-utilization
**Branch**: 4-full-core-utilization
**Status**: Planning

## Technical Context

- **Language**: C++20
- **Build system**: Meson (>=1.1)
- **Threading**: ThreadPool (custom, up to 8 threads), std::mutex
- **Key files to modify**:
  - `src/gallery.hpp` — Restructure members: remove single mutex, add completed queue
  - `src/gallery.cpp` — Rewrite load_thumbnails, draw, load_thumbnail, clear_thumbnails
- **Dependencies**: Existing ThreadPool class (no modifications needed)

## Architecture

### Current Flow (Serialized)
```
window_redraw() → draw(thumb1) [LOCK] → draw(thumb2) [LOCK] → ... → load_thumbnails() [LOCK, CANCEL ALL, REQUEUE]
                                                                        ↕ worker blocked on mutex
```

### New Flow (Parallel)
```
window_redraw() → drain_completed() [brief lock] → draw(thumb1) [NO LOCK] → draw(thumb2) [NO LOCK]

Navigation event → load_thumbnails()
                     ├─ drain_completed() [brief lock]
                     ├─ scan visible entries not in cache
                     └─ tpool.add() for missing [NO CANCEL]

Worker threads (load_thumbnail):
  decode image → scale thumbnail → push to completed [brief lock] → redraw
```

### Key Design Decisions

1. **No mutex in draw()**: Cache only modified on main thread. Workers never touch cache directly.
2. **No tpool.cancel() on frame**: Only cancel on navigation that changes visible set drastically (e.g., jump to different position).
3. **Producer-consumer for thumbnails**: Workers push to `completed` vector, main thread drains to `cache`.
4. **Incremental loading**: Only queue tasks for entries not already in cache, completed, or being actively loaded.

## File Structure

```
src/
├── gallery.hpp     # Remove: mutex, queue, active
│                   # Add: completed vector, completed_mutex
│                   # Add: drain_completed() method
│                   # Modify: load_thumbnails() → called from nav events
├── gallery.cpp     # Rewrite: load_thumbnails() — no cancel, incremental
│                   # Rewrite: draw() — remove lock_guard
│                   # Rewrite: load_thumbnail() — push to completed, not cache
│                   # Rewrite: clear_thumbnails() — no lock needed
│                   # Add: drain_completed() — move completed → cache
│                   # Modify: window_redraw() — drain before draw, don't call load_thumbnails
│                   # Modify: select(), activate() — call load_thumbnails
├── threadpool.hpp  # Unchanged
└── threadpool.cpp  # Unchanged
```

## Implementation Phases

### Phase 1: Add completed queue infrastructure
- Add `std::vector<ThumbEntry> completed` and `std::mutex completed_mutex` to Gallery
- Add `drain_completed()` method: lock completed_mutex, move entries to cache, unlock
- Add `is_loading(entry)` helper to check if entry is queued or active in tpool

### Phase 2: Decouple loading from render
- Remove `load_thumbnails()` call from `window_redraw()`
- Add `drain_completed()` call at start of `window_redraw()` (before drawing)
- Move `load_thumbnails()` call to: `activate()`, `select()`, `handle_imagelist()`
- Move `load_thumbnails()` call to: after `drain_completed()` in `window_redraw()` (to handle scroll-triggered visibility changes)

### Phase 3: Rewrite load_thumbnails() — incremental, no cancel
- Remove `tpool.cancel()` and `queue.clear()`
- Iterate visible entries (from layout scheme)
- For each entry: skip if in cache, skip if in completed, skip if in active tracking
- Only add new tasks for truly missing entries
- Track queued entries to avoid duplicates (use a `std::unordered_set<ImageEntryPtr>`)

### Phase 4: Rewrite load_thumbnail() — push to completed
- Remove lock_guard on `mutex` at entry (active insert)
- Remove lock_guard on `mutex` at exit (cache push, active erase)
- Instead: decode and scale without any lock
- At end: lock `completed_mutex`, push to `completed`, unlock
- Call `Application::redraw()`

### Phase 5: Remove mutex from draw() and clear_thumbnails()
- Remove `std::lock_guard lock(mutex)` from `draw()`
- Remove `std::lock_guard lock(mutex)` from `clear_thumbnails()`
- Both now only access `cache` which is main-thread-only
- Remove old `mutex`, `queue`, `active` members from Gallery

### Phase 6: Track active tasks without sets
- Use `std::atomic<size_t>` counter for active task count (optional, for progress display)
- Or keep a lightweight `std::unordered_set<ImageEntryPtr>` protected by `completed_mutex` for dedup checks

## Risk Assessment

| Risk | Mitigation |
|------|-----------|
| Cache read during worker push | Workers never touch cache; only push to completed |
| Duplicate tasks queued | Check cache + completed + queued set before adding task |
| Stale tasks for scrolled-away entries | Tasks complete and thumbnails cached; clear_thumbnails removes old entries |
| Memory growth from completed queue | drain_completed() runs every frame, queue stays small |
| tpool.cancel() still needed on deactivate | Keep cancel in deactivate() for mode switch cleanup |
