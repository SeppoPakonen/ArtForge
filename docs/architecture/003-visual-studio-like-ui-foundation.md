# Visual Studio-like UI Foundation Investigation

## Status

Native Win32 API and Windows Common Controls remain sufficient for the next
ArtForge UI step. No new GUI framework should be introduced now.

## Current Shell

`ArtForgeUiWin32` currently provides one shared scope shell used by the solution,
artist, series, and work applications. It creates:

- a top-level overlapped Win32 window
- a simple File menu
- a left Common Controls tree view
- a central static summary control
- a Common Controls status bar
- direct resize handling in the window procedure

This is enough for the existing developer-oriented placeholder apps, but the
layout logic is still embedded in `Shell.cpp` and should become reusable before
domain-specific work views are added.

## Feature Classification

| Feature | Classification | Notes |
| --- | --- | --- |
| Tree view | Common Controls available | Already used for scope navigation. Wrap creation, population, and sizing next. |
| List view / report table | Common Controls available | Use `WC_LISTVIEW` for history, diagnostics, layers, scenes, prompt layers, and search results. |
| Property grid-like panel | Easy custom Win32 wrapper | Start with a label/value two-column list view or grouped child controls. A full property-grid control is not required yet. |
| Split panes | Easy custom Win32 wrapper | Implement fixed or simple draggable splitters using child windows and resize math. Full docking is unnecessary now. |
| Tab controls | Common Controls available | Use `WC_TABCONTROL` for document sections or work-domain panels when needed. |
| Toolbar / menu / status bar | Common Controls available | Status bar already exists. Toolbar bands can use `TOOLBARCLASSNAME`; menu routing should be wrapped. |
| Docking panes | Major shell decision later | Defer full docking, floating panes, and persistence. Current mockups can be served by fixed panes and splitters. |
| Document tabs | Easy wrapper first | Start with tab control plus explicit content panes. Defer detachable/reorderable document tabs. |
| Graph / canvas view | Custom drawing / Direct2D later | Dependency graphs, pressure maps, image/layer canvases, and storyboards should be isolated behind custom view interfaces. |

## Scope Needs

### ArtForgeSolutionApp

Needs a solution explorer tree, a project/work list or report table, diagnostics
summary, world update panel, status bar, and later graph view. Common Controls
cover the near-term explorer, lists, tabs, toolbar, menu, and status bar.
Dependency graph visualization should be deferred to a Direct2D-backed custom
view.

### ArtForgeArtistApp

Needs identity and constraint summaries, recurring themes, projects, rules, and
source references. A tree view, report list, and property-style label/value
panel are enough for the next implementation tasks. Custom drawing can wait
until identity maps or relationship maps become active work.

### ArtForgeSeriesApp

Needs ordered works, arc pressure, missing slots, requirements, conflicts, and
project-level history. Use a tree or list view for ordered works, report lists
for diagnostics, and fixed split panes for overview/detail. No docking system is
needed yet.

### ArtForgeWorkApp: Lyrics / Music

Needs technical base text, section list, lyric line table placeholder, performer
profile reference, rule analytics, prompt actions, and history. Use tabs,
report lists, edit/static controls, and split panes first. A rich lyric editor
can be added later after the file model and history events stabilize.

### ArtForgeWorkApp: Visual Art Layers

Needs viewer layers, paint/construction layers, layer intent text,
first-glance/mid-depth/deep-meaning categories, references, and analytics. Use
tree/list controls for layer structure and property panels for metadata. The
actual image/canvas view should be a later custom Direct2D component.

### ArtForgeWorkApp: Dialog / Storyboard / Script

Needs scenes, blocks, time ranges, speaker or voice fields, dialogue/action
text, and storyboard placeholders. A tree or list view for scenes plus a
detail panel is enough for the next placeholder UI. Storyboard drawing and
timeline visualization should be custom views later.

## Recommended Next Step

Add a small reusable Win32 pane layout helper inside `ArtForgeUiWin32` before
adding work-domain UI modes. The helper should own child pane handles, standard
initial pane sizes, status-bar exclusion, and `WM_SIZE` layout math. It should
support a left navigation pane, center workspace, right detail pane, and bottom
status area. Placeholder controls are acceptable.

Do not implement docking, floating windows, custom canvas rendering, or document
tab persistence yet. Those features should wait until the pane layout and
domain-specific placeholder views reveal stable needs.

## Decision

Continue with native Win32 and Common Controls. Build a thin ArtForge-specific
shell wrapper layer next. Reserve Direct2D for future graph, canvas, pressure
map, and storyboard visualization work.
