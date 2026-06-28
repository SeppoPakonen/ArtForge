# Shell Redraw Regression Checklist

Run this checklist after changes to shell layout, splitters, tabs, command
states, pane visibility, or Common Controls wrappers.

## Apps Covered

Check all four scope apps:

- `ArtForgeSolutionApp`
- `ArtForgeArtistApp`
- `ArtForgeSeriesApp`
- `ArtForgeWorkApp`

For each app, test both startup modes:

- no command-line file path
- matching example scope file

## Startup Empty State

Expected behavior:

- command strip, explorer, center start page, inspector, bottom panel, and
  status bar appear without overlap
- center start page has Open/Recent/Example rows
- disabled command buttons remain visibly disabled

Failure symptoms:

- blank center area until resize
- old rows from a previous run
- controls overlapping at the default window size
- disabled buttons rendering as active

## Open File State

Expected behavior:

- opened path and load status update in center, inspector, bottom output, and
  status bar
- scope-specific document tabs appear
- explorer root expands and shows scope-specific nodes

Failure symptoms:

- stale no-file text remains after open
- explorer and inspector disagree about loaded state
- bottom panel shows outdated diagnostics

## Window Resize

Expected behavior:

- panes reflow as one shell
- command strip remains aligned
- list columns remain visible enough for scanning
- no parent background bands remain exposed

Failure symptoms:

- delayed blank strips after resizing
- child windows overlap
- right or bottom panes disappear before their clamp limits

## Maximize And Restore

Expected behavior:

- maximized layout preserves pane order and spacing
- restored layout uses persisted window and pane metrics when available
- status bar remains attached to the bottom edge

Failure symptoms:

- controls keep maximized dimensions after restore
- bottom panel overlaps the status bar
- splitters no longer match pane edges

## Splitter Drag

Test each splitter across the full clamped range:

- left explorer splitter
- right inspector splitter
- bottom output splitter

Expected behavior:

- cursor changes to the correct resize cursor
- mouse capture continues while dragging outside the splitter band
- panes resize continuously
- released capture leaves the shell in a normal cursor/paint state
- no stale vertical or horizontal bands remain

Failure symptoms:

- drag stops when the pointer leaves the splitter rectangle
- old pane content remains painted in uncovered regions
- panes overlap or detach from splitter bands
- cursor remains stuck as a resize cursor after release

## Tab Switching

Switch these tabs after resizing and splitter dragging:

- document tabs
- inspector tabs
- bottom Output, Tasks, Provider, and History tabs

Expected behavior:

- selected tab redraws immediately
- nested list/property controls stay inside the tab display area
- no old rows remain visible through the new tab

Failure symptoms:

- tab body remains blank until another resize
- old tab content remains visible
- nested list clips under the tab header

## Command Enable/Disable Changes

In WorkApp, open a work file and select/clear work rows.

Expected behavior:

- AI commands enable only when a selected row has prompt context
- Save remains disabled unless there are unsaved work changes
- Suggestion commands remain disabled until a pending suggestion is routed
- command strip grouping remains aligned after state changes

Failure symptoms:

- command buttons keep stale enabled state
- button group labels or separators drift after refresh
- disabled command text becomes clipped or unreadable

## Notes

Do not commit new binary screenshots for ordinary checklist runs. Add screenshots
only when they document a meaningful design milestone or a regression that
cannot be described clearly in text.

