# ArtForge Series File

## Format

- File role: project, album, or series scope
- Recommended extension: `.afseries`
- Storage: JSON
- Current schema version: `1`

An ArtForge series file describes an ordered project container such as an album,
visual series, scene set, or story arc. It references work items by relative
path.

## Minimal Fields

```json
{
  "format": "artforge.series",
  "schemaVersion": 1,
  "id": "series.sample",
  "scope": "series",
  "displayName": "Sample Series",
  "artist": {
    "id": "artist.sample",
    "path": "../../artists/sample.afartist"
  },
  "works": [
    {
      "id": "work.opening",
      "path": "works/01-opening/sample.afwork",
      "order": 1
    }
  ],
  "metadata": {}
}
```

## Rules

- `format` must be `artforge.series`.
- `schemaVersion` is a positive integer.
- `id` is stable within the workspace.
- `scope` must be `series`.
- `displayName` is human-facing English text by default.
- `artist.path` and `works[].path` should be relative paths.
- `works[].order` is optional but should be stable when present.
- `metadata` is optional and should remain JSON object data.
