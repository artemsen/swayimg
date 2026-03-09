# Feature Specification: Parallel Startup and Core Utilization

**Version**: 1.0
**Status**: Draft
**Created**: 2026-03-09

## Problem Statement

After the initial large-directory optimization (feature #2), startup still lags noticeably when opening directories with tens of thousands of files recursively. CPU cores remain underutilized — the process does not saturate available cores during startup, indicating insufficient parallelism in the current implementation.

### Root Cause Analysis

The current implementation has three serial bottlenecks that prevent full CPU utilization:

1. **Single-threaded directory scanning**: The background scanner (`scan_thread`) uses a single thread to recursively traverse all subdirectories. With 40,000+ files across 3,700+ directories, each requiring `stat()`, `file_size()`, and `last_write_time()` syscalls, this creates a long sequential bottleneck. Modern SSDs and kernel VFS caches can handle parallel filesystem operations efficiently.

2. **Sequential batch insertion under exclusive lock**: `flush_batch()` acquires a unique (exclusive) lock on the image list mutex to insert 100 entries at a time. During this lock, navigation and rendering are blocked, creating micro-stalls.

3. **First image display waits for synchronous scan of first level**: The `load()` function synchronously scans the first directory level (and descends deeper if no files are found at the top) before returning. For directories where the first image is buried deep in nested subdirectories, this initial synchronous phase can itself take seconds.

## Clarifications

### Session 2026-03-09
- Q: How many threads should be dedicated to directory scanning? → A: Reuse existing ThreadPool (up to 8 threads) for scan tasks, shared with preloader.
- Q: Is non-deterministic entry order during parallel scanning acceptable? → A: Yes, non-deterministic order during scan is acceptable; full sort is applied on completion.

## User Scenarios

### Scenario 1: Instant First Image
A user runs the viewer on `~/pic/` (40,000+ files, 3,700+ directories). The application should show the first discovered image as fast as possible — ideally under 500ms — while all CPU cores work in parallel to discover remaining files.

### Scenario 2: Rapid Scanning with Full Core Utilization
During background scanning of a large directory tree, the user opens a system monitor and sees multiple cores actively working. The scan completes significantly faster than single-threaded scanning.

### Scenario 3: Smooth Navigation During Parallel Scan
While multiple threads are scanning different parts of the directory tree, the user navigates through already-discovered images without stutter or freezes. Lock contention between scan threads and the UI thread is minimal.

### Scenario 4: Small Directory Unchanged
A user opens a directory with 20 images. The behavior is identical to current — no overhead from parallelism machinery.

## Functional Requirements

### FR1: Parallel Directory Scanning
Directory scanning must utilize the existing ThreadPool (up to 8 threads, shared with the image preloader) to scan different subdirectory branches in parallel. Each thread independently traverses a subtree of the directory hierarchy. Since scanning and preloading are sequential phases (preloading starts after first image is shown), the pool is effectively dedicated to scanning during startup.

### FR2: Minimal First-Image Latency
The application must begin scanning immediately and display the first discovered image as early as possible. The initial synchronous scan phase must be minimized — ideally returning the first image file found via a breadth-first or depth-first search of just one path, rather than scanning an entire directory level.

### FR3: Lock-Free or Low-Contention Entry Insertion
Batch insertion of discovered entries must minimize time spent holding exclusive locks. Approaches may include lock-free queues, finer-grained locking, or double-buffering to decouple producer (scan) threads from consumer (UI/navigation) threads.

### FR4: Work Stealing for Balanced Load
If some subtrees are much larger than others, idle scan threads should be able to pick up unprocessed directories from other threads' queues to balance load across cores.

### FR5: Graceful Shutdown
When the application exits during an active parallel scan, all scan threads must terminate promptly without resource leaks or crashes.

### FR6: Progress Feedback
The scanning indicator must accurately reflect total discovered entries across all scan threads. Updates should be aggregated to avoid excessive UI redraws.

## Non-Functional Requirements

### NFR1: CPU Utilization
During scanning of large directories (10,000+ files), CPU utilization should reach at least 50% of available cores on average. Peak utilization should approach 100% of assigned scan threads.

### NFR2: No Regression for Small Directories
For directories with fewer than 1,000 files, startup behavior and performance must be identical or better than current implementation. Thread pool overhead for small directories should be negligible.

### NFR3: Thread Safety
All concurrent access to the image list, path index, and associated data structures must be free of data races, deadlocks, and use-after-free. Lock ordering must be documented.

### NFR4: Memory Efficiency
Parallel scanning should not significantly increase memory usage. Per-thread batch buffers are acceptable but should be bounded.

## Success Criteria

- First image displayed within 500ms of launch for directories of any size (up to 200,000 files)
- Scan throughput at least 3x faster than single-threaded scanning for directories with 10,000+ files on a 4+ core system
- CPU utilization during scanning reaches at least 50% of available cores on average
- No UI freezes longer than 50ms during parallel scanning
- No performance regression for directories with fewer than 1,000 files
- Clean shutdown without resource leaks when exiting during active scan

## Assumptions

- Modern SSDs and kernel VFS caches can effectively serve parallel `stat()`/`readdir()` calls from multiple threads
- The existing `ThreadPool` class can be reused for scan parallelism
- `std::filesystem::directory_iterator` is thread-safe when different threads iterate different directories
- Lock contention on the image list is the primary source of micro-stalls, not the insertion itself

## Dependencies

- Existing `ThreadPool` class (`threadpool.hpp/cpp`)
- Existing async scanning infrastructure from feature #2 (background `scan_thread`, `flush_batch`, `scan_recursive`)
- Existing `shared_mutex` synchronization in `ImageList`

## Out of Scope

- Image decoding parallelism (already handled by ThreadPool-based preloader)
- Network filesystem optimizations
- Changing the underlying data structure for the image list (std::list)
- Gallery mode startup optimization (separate concern)
