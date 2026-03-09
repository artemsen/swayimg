# Feature Specification: Large Directory Performance

**Version**: 1.0
**Status**: Draft
**Created**: 2025-03-09

## Problem Statement

When opening a directory with a large number of files (e.g., `swayimg -r ~/pic/` with thousands of images), the application freezes and becomes unresponsive until the entire file list is built, sorted, and indexed. The user sees nothing — no window, no first image — until the full recursive directory scan completes. For directories with tens of thousands of files, this results in multi-second or even minutes-long startup delays with no visual feedback.

### Root Cause Analysis

Three blocking bottlenecks exist in the current startup path:

1. **Synchronous full directory scan**: `ImageList::load()` calls `add_dir()` recursively, collecting every file entry (including `stat()`, `file_size()`, and `last_write_time()` syscalls per file) before returning. Nothing is displayed until this completes.

2. **Duplicate detection via linear scan**: `add_entry()` performs `O(n)` linear search through the entire `std::list` for every new entry to check for duplicates, resulting in `O(n²)` total cost for n files.

3. **Full sort before first display**: The entire list must be sorted before the first image entry is returned, adding `O(n log n)` time on an already-large dataset before anything is shown.

4. **Per-file filesystem monitoring**: `FsMonitor::self().add(path)` is called for every single file and directory during scanning, adding inotify overhead that scales linearly.

## Clarifications

### Session 2025-03-09
- Q: What order should users see when navigating during an active background scan? → A: Discovery order (filesystem traversal order) during scan, full sort applied on completion.
- Q: When the background scan finishes and full sort is applied, what happens to the user's current viewing position? → A: Preserve current image — user stays on the same image, only its index/position number updates.

## User Scenarios

### Scenario 1: Quick Start with Large Recursive Directory
A user runs `swayimg -r ~/pic/` where `~/pic/` contains 50,000+ images across nested subdirectories. The application should display the first image within 1-2 seconds and continue building the file list in the background. The user can start browsing immediately while the list populates.

### Scenario 2: Gallery Mode with Progressive Loading
A user opens gallery mode on a large directory. Thumbnails for visible images load first; the total image count updates progressively as background scanning discovers more files. The gallery is usable before the full scan completes.

### Scenario 3: Navigation During Background Scan
While the file list is still being built in the background, the user navigates forward/backward through images. Navigation works smoothly with currently-discovered entries. When scanning completes, the full sorted list is available.

### Scenario 4: Sorting Large Lists
After the file list is fully built, changing sort order (e.g., from numeric to mtime) completes without freezing the UI, even for 100,000+ entries.

## Functional Requirements

### FR1: Asynchronous Directory Scanning
The file list must be built asynchronously. The application shall display the first discovered image and become interactive before the full directory scan completes. Directory scanning must run in a background thread.

### FR2: Incremental List Availability
Newly discovered image entries must become available for navigation as they are found, in filesystem discovery order. The user should be able to browse through already-discovered images while scanning continues. When the scan completes, the full sort is applied and the user's currently viewed image is preserved (only its index updates).

### FR3: Efficient Duplicate Detection
Duplicate file detection must not degrade to `O(n²)`. An index structure (e.g., hash set on file path) should reduce duplicate checking to amortized `O(1)` per insertion.

### FR4: Deferred Sorting
Initial display should not wait for the full list to be sorted. During scanning, entries are presented in filesystem discovery order. The full sort is performed once when scanning completes, in a background thread without blocking the UI. After sorting, the user's currently viewed image is preserved at its new sorted position.

### FR5: Batched Filesystem Monitoring
Filesystem watch registration should be batched per directory, not per file. Only directories need monitoring (the existing `FsMonitor::self().add(path)` per file is unnecessary overhead when directory-level monitoring already catches file changes).

### FR6: Progress Indication
During background scanning, the user should see feedback indicating that scanning is in progress (e.g., updating image count in the status bar).

### FR7: Responsive UI During All Operations
Sort operations, list rebuilds, and other bulk operations on large lists must not block the UI thread for more than one frame (~16ms). Long operations should be performed asynchronously with the result applied atomically.

## Non-Functional Requirements

### NFR1: Memory Efficiency
Background scanning should not require significantly more memory than the current approach. Entry structures should remain lightweight.

### NFR2: Thread Safety
The image list is already protected by `shared_mutex`. Background scanning must correctly synchronize with the UI thread and navigation operations without deadlocks or data races.

### NFR3: Backward Compatibility
Behavior for small directories (< 1,000 files) should remain identical. No existing configuration options or Lua API should break.

## Success Criteria

- First image is displayed within 2 seconds of launch, regardless of total directory size (up to 200,000 files)
- User can navigate through discovered images while background scan is still running
- Startup time for 50,000-file recursive directory scan is reduced by at least 5x compared to current blocking approach (measuring time-to-first-image)
- UI remains responsive (no freezes > 100ms) during directory scanning and sorting
- Gallery mode becomes usable (shows first page of thumbnails) before full scan completes
- Memory usage does not increase by more than 20% compared to current implementation

## Assumptions

- The existing `ThreadPool` infrastructure can be reused for background scanning
- `std::filesystem::directory_iterator` is efficient enough; no need for platform-specific directory APIs (e.g., `getdents64`)
- The `shared_mutex` on `ImageList` is sufficient for concurrent read/write access patterns (readers for navigation, single writer for scanning)
- Directory-level `FsMonitor` watches are sufficient to detect file additions/removals (no need for per-file watches)

## Dependencies

- Existing `ThreadPool` class (`threadpool.hpp/cpp`)
- Existing `FsMonitor` for filesystem monitoring
- Existing `shared_mutex` synchronization in `ImageList`

## Out of Scope

- Image decoding performance (already lazy-loaded on demand)
- Preloader/cache improvements (separate concern)
- Network filesystem optimizations (NFS, CIFS latency)
- Changing the image entry data structure from `std::list` to another container (could be a follow-up optimization)
