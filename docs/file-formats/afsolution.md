# ArtForge Solution File

## Format

- File role: solution scope
- Recommended extension: `.afsolution`
- Storage: JSON
- Current schema version: `1`

An ArtForge solution file is the top-level authored file for a modular creative
workspace. It references artist and project files by relative path. It is not a
bundled single-file product format.

## Minimal Fields

```json
{
  "format": "artforge.solution",
  "schemaVersion": 1,
  "id": "solution.sample",
  "scope": "solution",
  "displayName": "Sample ArtForge Solution",
  "artists": [
    {
      "id": "artist.sample",
      "path": "artists/sample.afartist"
    }
  ],
  "projects": [
    {
      "id": "series.sample",
      "path": "projects/sample-series/sample.afseries"
    }
  ],
  "metadata": {}
}
```

## Rules

- `format` must be `artforge.solution`.
- `schemaVersion` is a positive integer.
- `id` is stable within the workspace.
- `scope` must be `solution`.
- `displayName` is human-facing English text by default.
- `artists[].path` and `projects[].path` should be relative paths.
- `metadata` is optional and should remain JSON object data.

Future consumer packaging may bundle multiple files into one deliverable, but
that packaging layer must not replace this modular developer-phase format.
