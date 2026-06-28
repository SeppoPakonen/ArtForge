# UI Density and Scaling Notes

ArtForge currently targets a developer-oriented Windows desktop shell built with
native Win32 and Common Controls.

## Baseline

- Default validation baseline: Windows desktop scale at 100%.
- Practical secondary smoke target: Windows desktop scale at 125%.
- Minimum useful window size: about 720 by 420 pixels.
- Current default shell size: 720 by 420 pixels unless restored layout settings
  provide another size.
- Current pane defaults:
  - explorer width: 280 pixels
  - inspector width: 300 pixels
  - output pane height: 132 pixels
  - splitter width: 4 pixels
  - command strip height: 42 pixels
- Current pane clamps:
  - side panes clamp to at least 160 pixels while dragging
  - bottom pane clamps to at least 72 pixels while dragging
  - center document area keeps a minimum model width of 220 pixels

## Font and Row Policy

Controls use the system message font through `NONCLIENTMETRICSW`. The shell does
not hard-code a custom typeface. List and property rows use native Common
Controls behavior, with the property panel reserving a 24 pixel row-height
baseline.

Command strip labels are intentionally short. Long workflow descriptions belong
in status/output panels or docs, not inside compact toolbar buttons.

## High-DPI Expectations

The shell should remain usable at common desktop scales, but it does not yet
have a full per-monitor DPI implementation. Current metrics are fixed logical
pixel values. That is acceptable for the current developer phase, but deeper
visual polish should introduce explicit DPI-aware metric scaling before adding
large custom UI surfaces.

## Current Limitations

- Splitter hit targets are narrow at high scale because splitter width is fixed.
- Minimum pane sizes are fixed pixel values rather than DPI-scaled values.
- The command strip uses native button and static controls, not image-list icons.
- Manual visual validation is still required; there is no automated pixel test.

## Manual Smoke Checklist

Run these checks after shell layout or visual changes:

- 100% scale: launch all four apps with no file and with matching example files.
- 125% scale if practical: repeat launch and splitter checks.
- Small window: resize near the minimum useful size and verify controls do not
  overlap.
- Maximized window: verify pane proportions and restored splitter positions are
  reasonable.
- Splitter drag: drag left, right, and bottom splitters through their full
  clamped ranges and check for stale paint or clipped rows.
- Tab switching: switch document, inspector, and bottom tabs after resizing and
  dragging splitters.

