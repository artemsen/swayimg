# Tasks: Large Directory Performance

**Branch:** `2-large-dir-performance`
**Feature:** Reduce freezing and improve responsiveness when opening directories with large numbers of files
**Generated:** 2025-03-09

## User Stories

| Story | Description | Priority | Scenarios |
|-------|-------------|----------|-----------|
| US1   | Quick start with async scanning | P1 | S1, S3 |
| US2   | Progressive feedback during scan | P2 | S2 |
| US3   | Responsive sort on large lists   | P3 | S4 |

---

## Phase 1: Foundational (Blocking Prerequisites)

These tasks eliminate algorithmic bottlenecks that Phase 3+ depend on.

- [x] T001 Add `std::unordered_set<std::string> path_index` member to `ImageList` class in `src/imagelist.hpp`
- [x] T002 Replace O(n) linear duplicate check in `add_entry()` with O(1) `path_index` lookup in `src/imagelist.cpp`. Insert path string into `path_index` on successful add. Also update the public `add()` method's duplicate-aware path.
- [x] T003 Erase path from `path_index` in `ImageList::remove()` in `src/imagelist.cpp` to keep index in sync with entries list
- [x] T004 Remove per-file `FsMonitor::self().add(path)` call from `ImageList::add_file()` in `src/imagelist.cpp` (line ~507). Directory-level watches from `add_dir()` already cover file change events.

---

## Phase 2: Async Scanning Infrastructure [US1]

Goal: Show first image immediately, scan remaining files in background. User can navigate discovered images while scan runs. On completion, sort is applied preserving current image.

### Independent Test Criteria
- First image displays within 2 seconds for 50K+ recursive directory
- Navigation (next/prev) works during active scan without crash or freeze
- After scan completes, list is properly sorted and current image is preserved
- No data races or deadlocks under concurrent access

### Tasks

- [x] T005 [US1] Add scan state fields to `ImageList` in `src/imagelist.hpp`: `std::atomic<bool> scanning{false}`, `std::thread scan_thread`, and `bool is_scanning() const` accessor
- [x] T006 [US1] Add `ScanProgress` and `ScanComplete` event variants to `AppEvent` in `src/appevent.hpp`. `ScanProgress` holds `size_t count`. `ScanComplete` is empty.
- [x] T007 [US1] Add `handle_event()` declarations for `AppEvent::ScanProgress` and `AppEvent::ScanComplete` in `src/application.hpp`
- [x] T008 [US1] Implement `handle_event(ScanProgress)` and `handle_event(ScanComplete)` in `src/application.cpp`. Both call `redraw()`. `ScanComplete` also calls `current_mode()->activate()` or equivalent to refresh the current mode's state with the newly sorted list.
- [x] T009 [US1] Add event dispatch cases for `ScanProgress` and `ScanComplete` in the visitor pattern in `Application::handle_event()` variant dispatcher in `src/application.cpp`
- [x] T010 [US1] Implement `flush_batch(std::vector<ImageEntryPtr>&)` method in `src/imagelist.cpp`: acquire `unique_lock`, insert entries not in `path_index` into `entries` list and `path_index`, call `reindex()`, release lock
- [x] T011 [US1] Implement `scan_background(std::vector<std::filesystem::path>)` method in `src/imagelist.cpp`: recursively scan directories, accumulate entries in local vector, call `flush_batch()` every 100 entries, send `ScanProgress` event via `Application::self().add_event()` after each flush
- [x] T012 [US1] Implement `finish_scan()` method in `src/imagelist.cpp`: acquire `unique_lock`, sort entries using current order/comparator, call `reindex()`, set `scanning = false`, release lock, send `ScanComplete` event
- [x] T013 [US1] Rewrite `ImageList::load(const std::vector<std::filesystem::path>&)` in `src/imagelist.cpp`: synchronously scan first directory level (non-recursive) to find first entry and return it immediately. If `recursive` is true, collect subdirectories and spawn `scan_thread` running `scan_background()` with `scanning = true`. For non-recursive or non-directory sources, keep existing synchronous behavior. Sort synchronously only when no background scan is needed.
- [x] T014 [US1] Add cleanup in `ImageList` destructor or a `shutdown()` method in `src/imagelist.hpp` and `src/imagelist.cpp`: if `scan_thread.joinable()`, join it before destruction. Handle early application exit during active scan.
- [x] T015 [US1] Ensure `ImageList::get()` navigation methods in `src/imagelist.cpp` use `shared_lock` for reads and handle the case where the list is still growing (end-of-list during scan should not wrap/loop until scan is complete)

---

## Phase 3: Progressive Feedback [US2]

Goal: User sees scanning progress (updating file count) and a scanning indicator. Gallery mode shows thumbnails progressively as files are discovered.

### Independent Test Criteria
- Image count in status bar updates during scan
- Scanning indicator visible while scan is in progress, disappears on completion
- Gallery thumbnails load progressively as entries are discovered

### Tasks

- [x] T016 [US2] Add `{list.scanning}` text field in `src/text.cpp`: returns `"scanning..."` when `ImageList::self().is_scanning()` is true, empty string otherwise. Register the field alongside existing `{list.index}` and `{list.total}` fields.
- [x] T017 [US2] Update default viewer text scheme in `src/viewer.cpp` to append `{list.scanning}` to the top-right info block, e.g., `"Image: {list.index} of {list.total} {list.scanning}"`
- [x] T018 [US2] Update default gallery text scheme in `src/gallery.cpp` to append `{list.scanning}` to the top-right info block

---

## Phase 4: Responsive Sort [US3]

Goal: Changing sort order on a large list (100K+ entries) does not freeze the UI.

### Independent Test Criteria
- Sort order change on 100K entries completes without UI freeze > 100ms
- Current image preserved after re-sort
- No concurrent sort race conditions

### Tasks

- [x] T019 [US3] Modify `ImageList::set_order()` in `src/imagelist.cpp`: if `entries.size() > SORT_ASYNC_THRESHOLD` (5000), perform sort in a new detached thread (acquire `unique_lock`, sort, `reindex()`, release, send redraw event). Keep synchronous sort for small lists. Guard against concurrent sorts with a `std::atomic<bool> sorting{false}` flag.
- [x] T020 [US3] Add `std::atomic<bool> sorting{false}` field to `ImageList` in `src/imagelist.hpp`. Check this flag in `set_order()` to prevent concurrent sort operations.

---

## Phase 5: Polish & Cross-Cutting Concerns

- [x] T021 Verify `ImageList::load(const std::filesystem::path& list_file)` (load from file list) also benefits from path_index and works correctly with the new async flow in `src/imagelist.cpp`
- [x] T022 Verify `Application::handle_event(FileCreate)` in `src/application.cpp` works correctly during active background scan (new files discovered by FsMonitor while scan is running should not conflict with scanner thread)
- [x] T023 Verify Lua API `imagelist.enable_recursive()` in `src/luaengine.cpp` still works correctly — recursive flag must be set before `load()` is called, which is already the case in the current init order

---

## Dependencies

```
T001 ──┐
T002 ──┤ (path_index foundation)
T003 ──┤
       ├──► T010 ──► T011 ──► T013 (async scan core)
T004 ──┘    T005 ──► T013
            T006 ──► T007 ──► T008 ──► T009 (event infrastructure)
            T012 depends on T011
            T014 depends on T005
            T015 depends on T013

T016 depends on T005 (needs is_scanning())
T017 depends on T016
T018 depends on T016

T019 depends on T012 (reuses sort pattern)
T020 depends on T019

T021-T023 depend on all prior tasks
```

## Parallel Execution Opportunities

**Phase 1** (all independent, can run in parallel):
- T001 + T004 (different files/sections)
- T002 + T003 after T001

**Phase 2** (partial parallelism):
- T005 + T006 (different files: imagelist.hpp vs appevent.hpp)
- T007 + T008 after T006 (same file but different sections)
- T010 + T014 after T005

**Phase 3** (all parallel after T005):
- T016, T017, T018 are independent files

**Phase 4**:
- T019 + T020 (same logical change, do together)

## Implementation Strategy

**MVP (Phases 1-2):** T001-T015 deliver the core value — instant startup and background scanning. This alone achieves the 5x time-to-first-image improvement.

**Incremental additions:**
- Phase 3 (T016-T018): Polish — adds visual feedback during scan
- Phase 4 (T019-T020): Edge case — only matters for re-sort on very large lists
- Phase 5 (T021-T023): Verification — ensures no regressions in adjacent functionality

## Summary

| Metric | Value |
|--------|-------|
| Total tasks | 23 |
| Phase 1 (Foundational) | 4 tasks |
| Phase 2 (US1 - Async Scan) | 11 tasks |
| Phase 3 (US2 - Progress) | 3 tasks |
| Phase 4 (US3 - Async Sort) | 2 tasks |
| Phase 5 (Polish) | 3 tasks |
| Parallel opportunities | 8 task groups |
| MVP scope | T001-T015 (Phases 1-2) |
