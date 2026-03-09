# Data Model: Large Directory Performance

## Modified Entities

### ImageList (imagelist.hpp)

**Existing fields (unchanged):**
- `std::list<ImageEntryPtr> entries` — ordered list of image entries
- `std::shared_mutex mutex` — thread-safe access
- `Order order` — current sort order
- `bool reverse` — reverse flag
- `bool recursive` — recursive scanning flag
- `bool adjacent` — open adjacent files flag

**New fields:**
- `std::unordered_set<std::string> path_index` — O(1) duplicate detection by absolute path string
- `std::atomic<bool> scanning` — true while background scan is in progress
- `std::thread scan_thread` — dedicated background scanning thread

**Modified operations:**

| Operation | Before | After |
|-----------|--------|-------|
| `add_entry()` duplicate check | O(n) linear scan of `entries` list | O(1) lookup in `path_index` |
| `load()` | Synchronous: scan all → sort → return first | Hybrid: scan first level → return first → async scan rest |
| `sort()` | In-place sort under lock | Sort copy, atomic swap under brief lock |

**New operations:**
- `scan_async(sources)` — background thread entry point, scans and batch-inserts
- `flush_batch(vector<ImageEntryPtr>)` — insert accumulated entries under lock
- `finish_scan()` — sort, reindex, clear scanning flag, notify UI
- `is_scanning() const` — returns `scanning.load()`

### AppEvent (appevent.hpp)

**New event variants:**
- `ScanProgress { size_t total_found }` — periodic update during scan
- `ScanComplete {}` — scan finished, list is now sorted

### FsMonitor (no structural changes)

**Behavioral change only:**
- `add_file()` no longer calls `FsMonitor::self().add(path)` for individual files
- Only `add_dir()` registers directory watches (already does this)

## State Transitions

```
ImageList scanning state:
  IDLE --[load() called]--> SCANNING_FIRST
  SCANNING_FIRST --[first entry found]--> SCANNING_BACKGROUND (UI starts)
  SCANNING_BACKGROUND --[all dirs scanned]--> SORTING
  SORTING --[sort complete]--> IDLE

  At any point during SCANNING_BACKGROUND:
    - entries list grows (batch inserts)
    - navigation works on current entries
    - path_index grows in sync with entries
```

## Thread Access Patterns

| Thread | Reads | Writes |
|--------|-------|--------|
| UI (main/event loop) | entries (navigation, display), scanning flag | — |
| Scanner (background) | — | entries (batch insert), path_index, scanning flag |
| Gallery ThreadPool | entries (thumbnail iteration) | — |
| Viewer preloader | entries (get next entry) | — |

**Lock strategy:**
- Scanner: `unique_lock` during batch flush (brief, ~100 entries at a time)
- UI/Gallery/Preloader: `shared_lock` for reads
- Sort completion: `unique_lock` for atomic list swap
