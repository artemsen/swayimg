# Feature Specification: Fix UI Blocking During Image Loading

**Version**: 1.0
**Status**: Draft
**Created**: 2026-03-09

## Problem Statement

Despite image loading running in parallel via ThreadPool, the UI thread (display output and keyboard input) freezes during image opening operations. The main event loop is blocked by synchronous image loading calls and mutex contention with worker threads, making the application unresponsive for hundreds of milliseconds at a time.

### Root Cause Analysis

Investigation of the codebase reveals multiple synchronization bottlenecks that block the main event loop:

1. **Synchronous image loading in viewer**: When the user navigates to a new image and the preload cache is empty, `open_file()` calls `ImageLoader::load()` directly on the main thread. This performs full file I/O and decompression synchronously, blocking all event processing (keyboard, mouse, display) for 100-1000ms+ depending on image size.

2. **Gallery mutex contention**: Gallery's `load_thumbnails()` acquires a mutex that worker threads also need. Worker threads performing `load_thumbnail()` acquire the same mutex to update the cache. The main render thread and worker threads contend on this single mutex, causing the render path to stall while waiting for workers to release the lock.

3. **Frame callback blocking**: `lock_surface()` blocks the main thread waiting for the Wayland compositor's frame callback via `frame_mutex`. While waiting, no events (keyboard, scan progress, preload completion) can be processed.

4. **ThreadPool wait on mode switch**: `Gallery::deactivate()` calls `tpool.wait()` which blocks the main thread until all running thumbnail tasks complete, freezing UI for 100-500ms during mode transitions.

5. **Thumbnail task cancellation on every frame**: Gallery calls `tpool.cancel()` during every redraw, waiting for active tasks to complete before clearing the queue, creating repeated start-stop cycles that both waste CPU and block the main thread.

## User Scenarios

### Scenario 1: Navigating Images in Viewer Mode
A user presses arrow keys to browse through images. If the next image is not in the preload cache, the application freezes while loading it. During this freeze, additional key presses queue up but are not processed, and the display shows no feedback (no loading indicator, no response). The user perceives the application as "stuck."

**Expected behavior**: The application should remain responsive to input at all times. If an image is not yet loaded, the UI should continue responding to keyboard input and display updates while the image loads in the background.

### Scenario 2: Scrolling Gallery with Loading Thumbnails
A user scrolls through gallery mode while thumbnails are still loading. The display stutters and keyboard input is delayed because the render thread contends with thumbnail worker threads on a shared mutex. Holding down a scroll key causes visible freezes.

**Expected behavior**: Gallery scrolling should remain smooth regardless of thumbnail loading activity. Input should be processed without delay.

### Scenario 3: Switching Between Viewer and Gallery Modes
A user switches from gallery mode (with active thumbnail loading) to viewer mode. The application freezes while waiting for all thumbnail tasks to complete before transitioning.

**Expected behavior**: Mode switching should be near-instant. Background tasks should be cancelled without blocking the main thread.

### Scenario 4: Opening a Large Image File
A user opens a very large image (e.g., 50MP photo, multi-page TIFF). The entire application becomes unresponsive during the decode operation, with no visual feedback about loading progress.

**Expected behavior**: The application should show a loading state and remain responsive to input (e.g., user should be able to cancel or navigate away) while the image decodes.

## Functional Requirements

### FR1: Asynchronous Image Loading in Viewer
When the user navigates to an image that is not in the preload cache, the loading must happen asynchronously on a background thread. The main event loop must not block on image decoding. The viewer must continue displaying the previous image until the new image is ready, then trigger a redraw to show the new image. If the user navigates again while a background load is in progress, the in-flight load must be cancelled and only the latest requested image should be loaded. If an asynchronous load fails, the failed entry must be automatically removed from the image list and the next image must be loaded in the background, preserving current failure-handling behavior in an async context.

### FR2: Non-Blocking Gallery Thumbnail Access
The render path in gallery mode must be able to read thumbnail data without blocking on worker threads performing thumbnail decoding. Worker threads updating the thumbnail cache must not cause the render thread to stall.

### FR3: Non-Blocking Mode Transitions
Switching between viewer and gallery modes must not wait for background tasks to complete. Background tasks should be signaled to stop but the main thread must not block waiting for them to finish.

### FR4: Non-Blocking Frame Submission
The main event loop must be able to process input events and application events even when waiting for a Wayland frame callback. Frame synchronization must not prevent the event loop from handling queued events.

### FR5: Responsive Input Processing
Keyboard and mouse events must be processed within one frame interval (~16ms at 60Hz) under all conditions, including during image loading, thumbnail generation, and mode transitions.

### FR6: Eliminate Render-Path Thumbnail Loading
Thumbnail loading must not be initiated from within the render/draw path. The render path should only read from already-available cached data. Thumbnail loading should be driven by navigation/scroll events separately from rendering.

### FR7: Non-Blocking Task Cancellation
Cancelling background tasks (e.g., thumbnail loading, preloading) must return immediately without waiting for currently executing tasks to finish. Tasks should check a cancellation flag and abort early.

## Non-Functional Requirements

### NFR1: Input Latency
Keyboard and mouse input events must be processed within 32ms (two frame intervals at 60Hz) under all conditions, including during heavy image loading operations.

### NFR2: Frame Rate During Loading
The application must maintain at least 30 FPS during gallery scrolling while thumbnails are loading in the background.

### NFR3: Thread Safety
All changes to synchronization patterns must be free of data races, deadlocks, and use-after-free conditions. Lock ordering must be consistent to prevent deadlock.

### NFR4: Backward Compatibility
No changes to user-facing behavior, configuration options, or key bindings. The improvements are purely internal performance and responsiveness fixes.

## Success Criteria

- User input (keyboard/mouse) is processed within 32ms at all times, including during image loading
- No UI freeze longer than 50ms during any normal operation (navigation, scrolling, mode switching)
- Gallery scrolling maintains at least 30 FPS while thumbnails load in the background
- Mode switching (viewer to gallery and back) completes within 100ms from user perspective
- Large image loading (>20MP) does not block the UI; user can navigate away during loading
- Thumbnail worker threads and render thread do not contend on the same mutex during normal operation

## Clarifications

### Session 2026-03-09

- Q: What should the viewer display while an image loads asynchronously? → A: Keep showing the previous image until the new one is ready.
- Q: What happens when user navigates rapidly during async loading? → A: Cancel in-flight load, jump to the latest requested image.
- Q: How should async load failures be handled? → A: Auto-remove failed entry and load next image in background (same as current behavior, but async).

## Assumptions

- The Wayland compositor delivers frame callbacks at regular intervals (typically 60Hz)
- Image decoding libraries are thread-safe when decoding different images on separate threads
- The existing ThreadPool infrastructure supports non-blocking cancellation or can be modified to do so
- Displaying the previous image while the next one loads asynchronously is the expected behavior

## Dependencies

- Existing ThreadPool class (may need modification for non-blocking cancellation)
- Wayland frame callback mechanism
- Existing image preloader infrastructure in viewer
- Existing gallery thumbnail cache

## Out of Scope

- GPU/Vulkan rendering optimizations
- Image decoding speed improvements (SIMD, codec-level optimization)
- Multi-threaded Wayland event loop
- Network/remote filesystem I/O optimization
- Adding loading progress indicators to the UI (visual feedback is a separate feature)
