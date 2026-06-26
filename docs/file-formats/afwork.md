# ArtForge Work File

## Format

- File role: work item scope
- Recommended extension: `.afwork`
- Storage: JSON
- Current schema version: `1`

An ArtForge work file describes one creative work item, such as a song, image,
scene, lyric, or other unit. It references sources, fragments, rule files, and
history by relative path.

## Minimal Fields

```json
{
  "format": "artforge.work",
  "schemaVersion": 1,
  "id": "work.opening",
  "scope": "work",
  "workKind": "song",
  "displayName": "Opening Work",
  "series": {
    "id": "series.sample",
    "path": "../../sample.afseries"
  },
  "sources": [
    {
      "id": "source.note",
      "path": "sources/note.md"
    }
  ],
  "history": {
    "path": "history.afhistory.jsonl"
  },
  "metadata": {}
}
```

## Rules

- `format` must be `artforge.work`.
- `schemaVersion` is a positive integer.
- `id` is stable within the workspace.
- `scope` must be `work`.
- `workKind` is a small domain label such as `song`, `image`, or `scene`.
- `workDomain` may be added as a canonical domain discriminator such as
  `lyrics`, `visualArt`, or `scriptStoryboard`.
- `displayName` is human-facing English text by default.
- `series.path`, `sources[].path`, and `history.path` should be relative paths.
- `metadata` is optional and should remain JSON object data.
- Domain-specific payloads should live under `domain` and remain
  human-readable JSON. See [afwork-domains.md](afwork-domains.md).
