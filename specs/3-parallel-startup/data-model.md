# Data Model: Parallel Startup

## Modified Entities

### ImageList (modified)

Replaces single `scan_thread` with ThreadPool-based parallel scanning.

**New/modified fields**:
| Field | Type | Description |
|-------|------|-------------|
| scan_pool | ThreadPool | Thread pool for parallel directory scanning (replaces scan_thread) |
| scan_active | std::atomic<size_t> | Count of active scan tasks (0 = scan complete) |
| pending_entries | std::vector<ImageEntryPtr> | Shared buffer for entries from scan tasks (protected by pending_mutex) |
| pending_mutex | std::mutex | Protects pending_entries |
| scanning | std::atomic<bool> | Cancellation flag (unchanged) |

**Removed fields**:
| Field | Reason |
|-------|--------|
| scan_thread | Replaced by scan_pool |
| sorting | Replaced by scan_pool wait |

**State transitions**:
```
Idle → Scanning (load() called with recursive dirs)
  └→ scan_pool tasks running, scan_active > 0
  └→ pending_entries accumulates batches from tasks
  └→ periodic drain into entries list
Scanning → Finishing (scan_active reaches 0)
  └→ drain remaining pending_entries
  └→ sort under lock
  └→ reindex
Finishing → Idle (scanning = false, ScanComplete event)
```

### ThreadPool (unchanged)

No modifications needed. Tasks can add new tasks, which is the key property enabling recursive directory scanning via task decomposition.

### ImageEntry (unchanged)

No modifications to the entry structure itself.
