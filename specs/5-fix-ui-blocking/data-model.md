# Data Model: Fix UI Blocking During Image Loading

**Created**: 2026-03-09

## Overview

This feature modifies internal synchronization and state management. No new user-facing entities are introduced. The changes affect how existing data structures are accessed concurrently.

## Modified Entities

### Viewer::ImagePool

**Current state**:
- `Cache preload` — deque of preloaded images
- `Cache history` — deque of previously viewed images
- `ThreadPool tpool` — shared thread pool for preload tasks
- `std::atomic<bool> stop` — cancellation flag
- `std::mutex mutex` — single lock for both caches

**New fields**:
- `std::atomic<ImageEntryPtr> pending_target` — the image entry currently being loaded asynchronously (null when no async load in progress)
- `std::atomic<bool> loading` — whether an async load is in flight

**State transitions**:
- Idle → Loading: user navigates, cache miss, async load dispatched
- Loading → Loading: user navigates again, cancel previous, dispatch new
- Loading → Idle: async load completes (success or failure after retries exhausted)

### Gallery Synchronization

**Current state**:
- `std::mutex mutex` — protects cache, queue, and active sets
- `std::deque<ThumbEntry> cache`
- `std::set<ImageEntryPtr> queue`
- `std::set<ImageEntryPtr> active`

**New structure**:
- `std::shared_mutex cache_mutex` — protects thumbnail cache (shared reads, exclusive writes)
- `std::mutex task_mutex` — protects queue and active sets
- `std::atomic<bool> stopping` — non-blocking cancellation flag for worker tasks

**Invariants**:
- Lock ordering: `task_mutex` before `cache_mutex` (if both needed)
- Workers acquire `cache_mutex` exclusively only when inserting completed thumbnails
- Render path acquires `cache_mutex` shared (never blocks on workers reading cache)
- `stopping` flag checked by workers before cache insertion

### UiWayland Frame Sync

**Current state**:
- `std::mutex frame_mutex` — locked by render, unlocked by frame callback
- Blocking `lock()` in `lock_surface()`

**New behavior**:
- `try_lock()` instead of `lock()` in `lock_surface()`
- Frame callback signals readiness, render proceeds only when frame is available
- Skipped renders re-queue redraw event for next frame

## Thread Interaction Model

```
Main Thread (event loop via poll())
  ├── Processes keyboard/mouse events (never blocks on I/O)
  ├── Dispatches async image loads to ThreadPool
  ├── Renders frame only when frame callback ready (try_lock)
  └── Reads thumbnail cache (shared_mutex, shared lock)

Viewer Worker Threads (image_pool.tpool)
  ├── Load images asynchronously (ImageLoader::load)
  ├── Check stop flag before writing to cache
  └── Signal completion via Application::redraw()

Gallery Worker Threads (gallery.tpool, 4 threads)
  ├── Decode and render thumbnails
  ├── Check stopping flag before cache insertion
  ├── Acquire cache_mutex exclusively only for insertion
  └── Signal completion via Application::redraw()

Wayland Thread
  └── Frame callback unlocks frame_mutex
```
