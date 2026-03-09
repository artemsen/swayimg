# Feature Specification: Full CPU Core Utilization

**Version**: 1.0
**Status**: Draft
**Created**: 2026-03-09

## Problem Statement

Despite implementing parallel directory scanning (feature #3), the application still utilizes only one CPU core during typical usage — both in gallery mode and viewer mode. Multiple serialization bottlenecks prevent the existing thread pool infrastructure from achieving sustained parallel execution.

### Root Cause Analysis

A comprehensive codebase audit identified the following serialization points, ordered by impact:

1. **Gallery thumbnail loading inside render path**: Gallery's `window_redraw()` calls `load_thumbnails()` synchronously during every frame. This function acquires a mutex, cancels all pending thread pool tasks, re-queues new tasks, and blocks until coordination completes — all while holding the render lock. Worker threads cannot make progress because the main thread holds the mutex they need.

2. **Per-thumbnail mutex acquisition during drawing**: Each thumbnail drawn in gallery mode acquires the gallery mutex individually. For 20+ visible thumbnails per frame, this creates 20+ sequential lock acquisitions during a single render pass.

3. **Single-threaded render pipeline**: The entire render path (image compositing, scaling, anti-aliasing) executes on the main thread. While the software renderer uses a thread pool for pixel-level parallelism within a single image, gallery mode does not parallelize thumbnail rendering across multiple thumbnails.

4. **Blocking event loop architecture**: The main event loop processes redraw events synchronously. While rendering a frame, no other events (scan progress, preload completion, user input) are processed. This creates a pipeline stall where worker threads complete tasks but cannot deliver results until the render finishes.

5. **Thumbnail task cancellation on every frame**: Gallery calls `tpool.cancel()` during every redraw, which waits for all active tasks to complete before clearing the queue. This means worker threads that are decoding images are waited upon, then their results discarded, then new tasks are queued — creating a start-stop-start pattern that prevents sustained utilization.

6. **Image list lock contention**: During background scanning, the pending buffer mutex and main list shared_mutex create contention between scan threads, the main thread draining entries, and navigation operations.

## User Scenarios

### Scenario 1: Gallery Mode with Large Directory
A user opens gallery mode on a directory with 10,000+ images. Thumbnails should load progressively using all available CPU cores. Scrolling through the gallery should feel smooth, with thumbnails appearing rapidly as the user navigates.

### Scenario 2: Viewer Mode Image Preloading
A user browses through images in viewer mode. The next N images should decode in parallel on background threads while the current image is displayed. Switching to the next image should be instant because it was preloaded.

### Scenario 3: Startup with Recursive Directory
A user opens a large recursive directory. Directory scanning, first image display, and gallery list building should all happen concurrently, utilizing multiple cores from the moment the application starts.

### Scenario 4: Sustained Multi-Core Usage
During any operation involving multiple images (gallery browsing, preloading, scanning), a system monitor should show multiple cores actively working, not just brief spikes followed by idle periods.

## Functional Requirements

### FR1: Parallel Gallery Thumbnail Decoding
Gallery mode must decode visible thumbnails in parallel using multiple worker threads. Thumbnail decode tasks must not be cancelled and re-queued on every frame redraw. Instead, previously queued tasks should continue executing, and only newly visible thumbnails should be added to the queue.

### FR2: Non-Blocking Thumbnail Rendering
Thumbnail rendering in gallery mode must not hold a mutex for the entire draw operation. Completed thumbnails should be stored in a cache that the render path can read without blocking ongoing decode tasks.

### FR3: Decouple Thumbnail Loading from Render
Thumbnail loading must not be triggered from within the render path. Instead, thumbnail loading should be driven by scroll/navigation events, and the render path should only read from an already-populated cache.

### FR4: Reduce Lock Granularity in Gallery
The gallery's single mutex protecting both the thumbnail cache and task coordination must be split into finer-grained locks or replaced with lock-free patterns so that worker threads completing thumbnail decodes do not block on the render path and vice versa.

### FR5: Eliminate Redundant Task Cancellation
The gallery must not cancel all pending thumbnail tasks on every frame. Tasks for thumbnails that are still visible should continue executing. Only tasks for thumbnails that have scrolled completely out of view should be cancelled.

### FR6: Reduce Image List Lock Contention
During background scanning, lock contention between scan threads, the drain operation, and navigation must be minimized. Batch sizes and drain frequency should be tuned to reduce lock hold time.

### FR7: Sustained Worker Thread Utilization
Worker threads must spend the majority of their time executing useful work (image decoding, thumbnail generation) rather than waiting on mutexes, being cancelled, or idling between task batches.

## Non-Functional Requirements

### NFR1: CPU Utilization Target
During gallery thumbnail loading of large directories (1,000+ images), average CPU utilization across all available cores should reach at least 60% until all visible thumbnails are loaded.

### NFR2: Gallery Scroll Smoothness
Gallery scrolling must maintain at least 30 frames per second with no individual frame taking longer than 50ms to render, even while thumbnails are loading in the background.

### NFR3: No Data Races
All changes to lock granularity and threading patterns must be free of data races, deadlocks, and use-after-free conditions.

### NFR4: Backward Compatibility
No changes to user-facing behavior, configuration options, or Lua API. The improvements are purely internal performance optimizations.

## Success Criteria

- During gallery thumbnail loading, at least 4 CPU cores are actively utilized for sustained periods (>1 second)
- Gallery becomes visually responsive (first page of thumbnails visible) within 2 seconds for directories with 1,000+ images
- Thumbnail loading throughput is at least 3x faster than current single-core-dominated implementation
- No UI freezes longer than 50ms during gallery scrolling while thumbnails load
- Viewer mode image preloading achieves sustained multi-core decode when preload limit >= 4

## Assumptions

- Image decoding libraries (libjpeg, libpng, libwebp, etc.) are thread-safe when decoding different images on different threads
- The existing ThreadPool infrastructure is correct and can support the required task patterns
- Gallery thumbnail cache can fit all visible thumbnails in memory without issues
- Wayland frame callback timing does not inherently limit parallelism

## Dependencies

- Existing ThreadPool class
- Parallel directory scanning from feature #3
- Existing gallery thumbnail cache infrastructure
- Existing viewer preloader infrastructure

## Out of Scope

- Vulkan rendering path parallelism (already uses GPU)
- Wayland protocol-level optimizations
- Image decoding algorithm improvements (e.g., SIMD)
- Changing the Wayland event loop to be multi-threaded
