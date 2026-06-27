# Current UI Gap and Target Shell Baseline

ArtForge currently has a functional native Win32/Common Controls shell for the
Solution, Artist, Series, and Work apps. The shell is useful for load-status
checks and early model wiring, but it still reads as a debug harness rather than
a credible Visual Studio-like production tool.

This document defines the near-term UI shell target. It does not authorize a new
GUI framework, docking system, floating pane model, or custom canvas.

## Current Gap

All four scope apps share the same basic shape:

- A top menu with a small File command set.
- A left tree that exposes mostly raw scope structure.
- A central area that is either a static text dump or a WorkApp table.
- A right-side Details list that behaves like a raw list view.
- A status bar with load status.

The main credibility gaps are:

- Weak command surface: common actions such as Open, Save, Refresh, prompt
  generation, queueing, and suggestion review are not visible as a command
  rhythm.
- Static central dump: non-work scopes show plain static text rather than a
  document/start page surface.
- Plain empty state: launching without a file path does not look intentional or
  guide the user toward the next action.
- Low hierarchy: panes have little visual distinction, no stable headers, and
  minimal spacing rules.
- No document-tab rhythm: there is no consistent Start / Current Scope document
  model across apps.
- No output/task panel rhythm: diagnostics, provider status, history, and queue
  activity compete with inspector rows instead of living in a bottom panel.
- Raw right-side list view: Details is technically useful, but it lacks property
  sections, readable empty values, and inspector-oriented sizing.

## Target Baseline

The target remains native Win32 and Windows Common Controls through
`ArtForgeUiWin32`. Views render presentation models; they must not interpret
domain files directly when an adapter can provide the required labels or rows.

The shared shell baseline should contain:

- Menu: top-level application menu for stable command categories.
- Command strip: visible Open, Save, Refresh, Build Prompt, Queue Manual AI
  Task, Accept Suggestion, and Reject Suggestion commands, enabled according to
  the current presentation state.
- Navigation explorer: left tree with intentional roots, empty-state nodes, and
  scope-specific grouping.
- Document area: central Start and Current Scope tabs, with WorkApp domain
  views integrated into the same tab rhythm.
- Inspector: right property panel with stable sections such as Scope,
  Selection, File, Provider, and Diagnostics.
- Output/task panel: bottom read-only panel for Output, Tasks, Provider, and
  History summaries.
- Status bar: compact current state, file load result, or latest command result.

The first iteration should use standard controls, system fonts, consistent
spacing, and simple text labels. Icons can remain placeholders until the command
and pane structure is stable.

## Scope Expectations

### Solution App

The Solution app should feel like the top-level workspace. Its navigation tree
should group artists, series/projects, works, dependencies, and diagnostics. The
central Start tab should explain the expected solution file path and show the
loaded solution summary. The bottom panel should surface load issues, project
graph diagnostics, and future dependency output.

### Artist App

The Artist app should focus on artist identity, constraints, projects, rules,
and references. Empty state should still show an intentional explorer with
Current Scope, Open Recent, Examples, and Diagnostics placeholders. The
inspector should expose artist file status and selected navigation details
without dumping raw file fields.

### Series App

The Series app should group parent artist context, ordered works, requirements,
and series-level diagnostics. The document area should distinguish the Start
page from the current series summary. The bottom panel should be ready for work
ordering and dependency messages without implementing a full solver.

### Work App

The Work app already has more data-backed UI than the other apps. It should keep
the existing domain table, prompt preview, suggestion review, and provider
status foundations, but integrate them into a shared document-tab and bottom
panel rhythm. Work item editing remains limited to existing command-routed
flows.

## Out of Scope

The following are intentionally deferred:

- Docking or floating panes.
- A third-party GUI framework.
- A custom Direct2D document canvas.
- Pixel-perfect automated visual tests.
- Full editor UI work beyond existing command-routed placeholders.
