# Resizable Shell Splitters

Task: `plan/05-ui-shells/029-add-resizable-shell-splitters.md`

The shared Win32 shell supports native mouse resizing for these view-owned pane
metrics:

- Explorer width between the navigation tree and document tabs.
- Inspector width between the document tabs and inspector tabs.
- Bottom panel height between the document area and output/task tabs.

The splitters are intentionally local to `ArtForgeUiWin32`. Presentation models
and services remain independent of `HWND` and pixel layout.

## Manual Smoke Checks

Run each scope app and drag all three splitter regions:

- `ArtForgeSolutionApp.exe examples\graph\sample.afsolution.json`
- `ArtForgeArtistApp.exe examples\graph\artists\sample.afartist.json`
- `ArtForgeSeriesApp.exe examples\graph\projects\sample-series\sample.afseries.json`
- `ArtForgeWorkApp.exe examples\work-domains\lyrics.afwork.json`

Expected behavior:

- The cursor changes to horizontal resize on the explorer/document and
  document/inspector splitters.
- The cursor changes to vertical resize above the bottom panel.
- Dragging resizes panes live.
- Panes clamp before becoming unusable.
- No docking, floating, or external UI framework is introduced.
