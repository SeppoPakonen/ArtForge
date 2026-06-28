# Pane Captions and Section Headers

The current shell uses native controls for visual hierarchy:

- the navigation tree has an `Explorer` root
- the center tab uses `Start Page` before a file is loaded and `Scope Overview`
  after a file is loaded
- the inspector and related tabs use title case labels
- the bottom panel keeps `Output`, `Tasks`, `Provider`, and `History` tabs
- property panel group rows are plain section headers instead of debug-style
  marker text

This keeps the shell visually clearer without adding owner-drawn pane chrome or
a Ribbon surface. Future polish can add native caption controls if the shell
needs more distinction than tabs and list headers provide.

