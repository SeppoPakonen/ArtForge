# ArtForge Artist File

## Format

- File role: artist scope
- Recommended extension: `.afartist`
- Storage: JSON
- Current schema version: `1`

An ArtForge artist file stores artist identity, constraints, and references to
rules or projects owned by that artist. Human-authored rules remain Markdown
files referenced from this JSON file.

## Minimal Fields

```json
{
  "format": "artforge.artist",
  "schemaVersion": 1,
  "id": "artist.sample",
  "scope": "artist",
  "displayName": "Sample Artist",
  "ruleFiles": [
    "rules/artist.md"
  ],
  "projects": [
    {
      "id": "series.sample",
      "path": "../projects/sample-series/sample.afseries"
    }
  ],
  "metadata": {}
}
```

## Rules

- `format` must be `artforge.artist`.
- `schemaVersion` is a positive integer.
- `id` is stable within the workspace.
- `scope` must be `artist`.
- `displayName` is human-facing English text by default.
- `ruleFiles[]` and `projects[].path` should be relative paths.
- `metadata` is optional and should remain JSON object data.
