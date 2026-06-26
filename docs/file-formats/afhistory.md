# ArtForge History Event Log

## Format

- File role: persistent scope history
- Recommended extension: `.afhistory.jsonl`
- Storage: append-only JSON Lines
- Current schema version: `1`

ArtForge history is separate from Git. Git remains the repository checkpoint and
collaboration tool; ArtForge history records high-frequency creative actions
that can later support browse, undo, redo, compare, and branch workflows.

Each line is one JSON object. New events should be appended rather than editing
earlier lines. Snapshot files may be referenced from events but are stored
separately as JSON under a `snapshots/` directory.

## Minimal Event Fields

```json
{
  "id": "hist.sample.0001",
  "timestamp": "2026-06-25T00:00:00Z",
  "actor": "user",
  "scope": "work",
  "operation": "user text edit",
  "summary": "Edited opening lyric draft",
  "affected_files": [
    "work.afwork",
    "drafts/opening.md"
  ],
  "before": "snapshots/000001.json",
  "after": "snapshots/000002.json",
  "prompt_package_id": "",
  "ai_result_id": ""
}
```

Snapshot metadata can be recorded as a normal JSONL history event with
`operation` set to `snapshot created`. The snapshot content itself stays in an
adjacent JSON file; the event records metadata only:

```json
{
  "operation": "snapshot created",
  "after": "snapshots/000002.json",
  "snapshot": {
    "id": "snapshot.sample.0002",
    "summary": "Opening work after first text edit",
    "timestamp": "2026-06-25T00:05:00Z",
    "related_event_id": "hist.sample.0001"
  }
}
```

Branch placeholders can also be stored as JSONL metadata events. They reserve
stable branch terminology for later UI and restoration work, but do not switch
or restore history:

```json
{
  "operation": "branch created",
  "branch": {
    "id": "branch.alt-chorus",
    "parent_branch_id": "branch.main",
    "creation_reason": "Explore a lower-energy chorus"
  }
}
```

Scope applications may also append non-fatal file operation events to
`operations.afhistory.jsonl` beside the opened scope file. These events record
basic UI/file operations without implementing undo or redo:

- `file open attempted`
- `file load succeeded`
- `file load failed`
- `file save succeeded`

## Rules

- `id` is stable and unique within the history file.
- `timestamp` should be an ISO 8601 UTC string.
- `actor` is `user`, `AI`, or `system`.
- `scope` is `solution`, `artist`, `series`, `work`, or `fragment`.
- `operation` uses the operation vocabulary represented in `ArtForgeHistory`.
- `summary` is short human-facing English text.
- `affected_files[]` uses relative paths.
- `before`, `after`, `prompt_package_id`, and `ai_result_id` may be empty when
  not relevant.
- `snapshot` is required for `snapshot created` events and contains only
  metadata: id, summary, timestamp, and related event id.
- `branch` is required for `branch created` events and contains only placeholder
  metadata: id, optional parent branch id, and creation reason.

This format does not implement a full undo/redo engine, branching engine,
snapshot restoration, or UI browser yet.
