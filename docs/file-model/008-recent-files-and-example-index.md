# Recent Files And Example Index

Task: `plan/02-file-model/008-add-recent-files-and-example-index.md`

ArtForge stores recent files in a user-local JSON file:

```text
%APPDATA%\ArtForge\settings\recent-files.json
```

The file is outside project directories and examples. It contains human-readable
entries with:

- `path`
- `scope`
- `displayName`
- `lastOpenedUtc`

Missing recent files are skipped on load and reported through diagnostics.

The example index is service-level and UI-neutral. It currently lists the
checked-in sample solution, artist, series, and work-domain files from
`examples/`.

Smoke validation:

```powershell
.\bin\Debug\x64\ArtForgeWorkApp.exe --smoke-recent-files-example-index
```
