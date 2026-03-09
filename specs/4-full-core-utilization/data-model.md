# Data Model: Full CPU Core Utilization

## Modified Entities

### Gallery (modified)

**Current state**: Single `mutex` protects `cache`, `queue`, `active`. `load_thumbnails()` called from render path, cancels all tasks every frame.

**New state**: Producer-consumer pattern. Workers push to `completed` queue. Main thread drains before render.

**Modified fields**:
| Field | Change | Description |
|-------|--------|-------------|
| mutex | REMOVE | No longer needed (cache accessed only from main thread) |
| queue | REMOVE | Replaced by ThreadPool's internal queue |
| active | REMOVE | Replaced by ThreadPool's active tracking |
| completed | ADD | `std::vector<ThumbEntry>` — completed thumbnails from workers |
| completed_mutex | ADD | `std::mutex` — protects completed vector |

**New flow**:
```
Navigation event → load_thumbnails()
  ├─ drain completed thumbnails into cache
  ├─ determine newly visible entries not in cache
  └─ enqueue decode tasks for missing entries (no cancel)

Worker thread (load_thumbnail):
  ├─ decode image (no locks held)
  ├─ create thumbnail (no locks held)
  ├─ push to completed (brief completed_mutex)
  └─ trigger redraw

Render (window_redraw):
  ├─ drain completed thumbnails into cache (brief)
  ├─ draw thumbnails from cache (NO locks)
  └─ clear_thumbnails (NO locks, cache only on main thread)
```

### ThreadPool (unchanged)

No modifications needed.
