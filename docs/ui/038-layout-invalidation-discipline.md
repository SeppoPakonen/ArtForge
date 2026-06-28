# Layout Invalidation Discipline

ArtForge shell layout is owned by `ArtForgeUiWin32`. Application services and
presentation models do not know about `HWND`, rectangles, or paint timing.

## Reflow Path

Use the shared shell reflow path for every shell geometry change:

- recompute the command bar, explorer, document tab area, inspector, bottom
  panel, and splitter rectangles from the current client area
- clamp pane sizes before applying child window positions
- move the major pane windows through the shared pane layout helper
- move nested list/property controls from their tab display areas
- invalidate the parent shell and child controls when an interaction exposes
  background immediately

`WM_SIZE`, splitter dragging, and tab selection changes must use this same path.
New pane show/hide behavior should be routed through it instead of adding
one-off `MoveWindow`, `InvalidateRect`, or `RedrawWindow` calls in command
handlers.

## Invalidation Rules

Use `InvalidateRect` when a window can repaint on the normal message loop.
Use `RedrawWindow` only when the user interaction can expose stale background or
old child content immediately, such as splitter dragging, tab selection, or a
visible panel mode switch.

Child controls should be invalidated with the parent after shell geometry
changes. Common Controls can otherwise keep old list rows or tab interiors
visible until the next incidental paint.

## Manual Smoke Checklist

Run this checklist after shell layout changes:

- start each of Solution, Artist, Series, and Work apps with no file
- open the matching example file for each app
- resize the window slowly and quickly from each edge
- maximize and restore the window
- drag the left explorer splitter across its full clamped range
- drag the right inspector splitter across its full clamped range
- drag the bottom output splitter across its full clamped range
- switch document, inspector, and bottom tabs after each drag
- verify no stale bands, overlapping child controls, delayed blank areas, or
  clipped list rows remain visible

