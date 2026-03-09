# Implementation Plan: Large Directory Performance

**Branch:** `2-large-dir-performance`
**Spec:** [spec.md](spec.md)
**Research:** [research.md](research.md)
**Data Model:** [data-model.md](data-model.md)

## Technical Context

- **Language/Standard:** C++20
- **Build system:** Meson (>=1.1)
- **Threading:** `std::thread`, `std::shared_mutex`, `std::atomic`, existing `ThreadPool`
- **Event loop:** `poll()`-based in `Application::event_loop()`
- **Key files:**
  - `src/imagelist.hpp/cpp` — core changes (async scan, path index, deferred sort)
  - `src/fsmonitor.cpp` — remove per-file watches (1 line change)
  - `src/application.hpp/cpp` — new event handlers for scan progress/completion
  - `src/appevent.hpp` — new event variants
  - `src/viewer.cpp` — handle scan completion (preserve current image)
  - `src/gallery.cpp` — handle growing list during scan

## Implementation Phases

### Phase 1: Efficient Duplicate Detection (FR3)

**Goal:** Eliminate O(n²) bottleneck in `add_entry()`.

**Changes:**
1. **imagelist.hpp**: Add `std::unordered_set<std::string> path_index` member
2. **imagelist.cpp `add_entry()`**: Replace `std::find_if` linear scan with `path_index.count(path_string)` check. Insert path into `path_index` on successful add.
3. **imagelist.cpp `remove()`**: Also erase from `path_index`

**Risk:** Low — isolated change, no threading implications yet.
**Testing:** Load a directory with duplicate symlinks, verify no duplicates and no O(n²) behavior on 50K+ files.

### Phase 2: Remove Per-File Filesystem Watches (FR5)

**Goal:** Eliminate unnecessary inotify overhead.

**Changes:**
1. **imagelist.cpp `add_file()`**: Remove `FsMonitor::self().add(path)` call (line 507). Directory-level watches from `add_dir()` already cover file events.

**Risk:** Low — directory watches already detect file changes via `IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVE`. Verify with test: modify a file in a watched directory, confirm event is still received.
**Testing:** Open directory, modify/add/remove files, verify FsMonitor events still fire correctly.

### Phase 3: Asynchronous Directory Scanning (FR1, FR2, FR4)

**Goal:** Show first image immediately, continue scanning in background.

This is the core change. Split into sub-steps:

#### 3a: Add scan state and events

**Changes:**
1. **imagelist.hpp**: Add `std::atomic<bool> scanning{false}` and `std::thread scan_thread`
2. **appevent.hpp**: Add `ScanProgress { size_t count }` and `ScanComplete {}` event variants
3. **application.cpp**: Add `handle_event()` overloads for new events — both trigger `redraw()`
4. **imagelist.hpp**: Add `bool is_scanning() const` accessor
5. **imagelist destructor/cleanup**: Join `scan_thread` if joinable

#### 3b: Split `load()` into sync-first + async-rest

**Changes to `imagelist.cpp`:**

1. **New `load()` flow:**
   ```
   load(sources):
     lock()
     for each source:
       if directory:
         scan first level only (no recursion) → find first valid entry
         if recursive:
           queue directory for background scan
       else:
         add_file() as before
     unlock()

     if background_dirs not empty:
       scanning = true
       scan_thread = thread([dirs, this] { scan_background(dirs) })
     else:
       sort()  // small list, sort synchronously

     return first_entry
   ```

2. **New `scan_background()` method:**
   ```
   scan_background(dirs):
     local_batch = vector<ImageEntryPtr>
     for each dir in dirs:
       recursive_scan(dir, local_batch)
       if local_batch.size() >= BATCH_SIZE (100):
         flush_batch(local_batch)
         send ScanProgress event
         local_batch.clear()

     flush remaining batch
     finish_scan()
   ```

3. **New `flush_batch()` method:**
   ```
   flush_batch(batch):
     unique_lock(mutex)
     for entry in batch:
       if !path_index.count(entry.path):
         entries.push_back(entry)
         path_index.insert(entry.path)
     reindex()
     unlock()
   ```

4. **New `finish_scan()` method:**
   ```
   finish_scan():
     // sort in this thread on a copy, then swap
     unique_lock(mutex)
     entries.sort(comparator)
     reindex()
     scanning = false
     unlock()
     send ScanComplete event
   ```

**Risk:** Medium — threading correctness is critical. The `shared_mutex` must protect all list access. Navigation during scan must handle a growing list without invalidating iterators (`std::list` iterators are stable on insert).

#### 3c: Handle scan completion in viewer

**Changes to `viewer.cpp`:**
1. On `ScanComplete` event: The current `image->entry` is still valid (shared_ptr). Its `index` field is updated by `reindex()`. Trigger info text refresh to show updated position.

**Changes to `application.cpp`:**
1. `handle_event(ScanComplete)`: Call `current_mode()->on_list_changed()` or equivalent, then `redraw()`

### Phase 4: Progress Indication (FR6)

**Goal:** Show scanning status in the info overlay.

**Changes:**
1. **text.cpp**: Add a `{list.scanning}` field that shows "scanning..." or empty string based on `ImageList::self().is_scanning()`
2. **viewer.cpp / gallery.cpp**: The existing `{list.total}` field already calls `ImageList::self().size()` which will return the current (growing) count. During scan, `{list.total}` naturally increases on each redraw triggered by `ScanProgress`.
3. Optionally append scanning indicator to the default text scheme: `"Image: {list.index} of {list.total} {list.scanning}"`

**Risk:** Low — purely display change.

### Phase 5: Responsive Sort Operations (FR7)

**Goal:** Ensure re-sort (e.g., user changes sort order via Lua) doesn't freeze UI on large lists.

**Changes:**
1. **imagelist.cpp `set_order()`**: If list size > threshold (e.g., 5000 entries), perform sort in a detached thread:
   ```
   set_order(new_order):
     order = new_order
     if entries.size() > SORT_ASYNC_THRESHOLD:
       thread([this] {
         unique_lock(mutex)
         entries.sort(comparator)
         reindex()
         unlock()
         send redraw event
       }).detach()
     else:
       sort()  // synchronous for small lists (backward compat)
   ```

**Risk:** Low-medium — must ensure no concurrent sort operations. Use the `scanning` flag or a separate `sorting` flag to gate.

## Dependency Order

```
Phase 1 (duplicate detection) ← no dependencies
Phase 2 (fs monitor)          ← no dependencies
Phase 3 (async scan)          ← depends on Phase 1 (needs path_index for batch insert)
Phase 4 (progress)            ← depends on Phase 3 (needs scanning flag and events)
Phase 5 (async sort)          ← depends on Phase 3 (reuses async pattern)
```

Phases 1 and 2 can be implemented in parallel. Phase 3 is the critical path.

## Testing Strategy

| Test | Method | Success Criterion |
|------|--------|-------------------|
| Duplicate detection | Symlink directory loop, 50K files | No O(n²), completes in < 5s |
| FsMonitor correctness | Modify file in watched dir | Event received without per-file watch |
| Time-to-first-image | `time swayimg -e 'imagelist.enable_recursive(true)' ~/pic/ &; sleep 2; kill %1` | Window visible in < 2s for 50K files |
| Navigation during scan | Open large dir, immediately press next/prev | Images navigate without crash or freeze |
| Sort completion | Wait for scan to finish, verify sorted order | List sorted correctly, current image preserved |
| Gallery progressive | Open gallery on large dir | Thumbnails appear before scan completes |
| Memory overhead | Compare RSS before/after with 100K files | < 20% increase |
| UI responsiveness | Profile frame times during scan | No frame > 100ms |

## Files Modified (Summary)

| File | Changes |
|------|---------|
| `src/imagelist.hpp` | Add path_index, scanning flag, scan_thread, new methods |
| `src/imagelist.cpp` | Rewrite load(), add_entry(), new scan_background/flush/finish methods |
| `src/fsmonitor.cpp` | — (no changes needed, already per-directory) |
| `src/imagelist.cpp` | Remove FsMonitor::add() call from add_file() |
| `src/appevent.hpp` | Add ScanProgress, ScanComplete event types |
| `src/application.hpp` | Add handle_event() declarations for new events |
| `src/application.cpp` | Add handle_event() implementations for new events |
| `src/text.cpp` | Add {list.scanning} field |
| `src/viewer.cpp` | Handle scan completion (refresh info text) |
