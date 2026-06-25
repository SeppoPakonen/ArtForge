# Architecture Decision: Native Win32 GUI Stack

## Status

Accepted for the initial developer-oriented ArtForge UI phase.

## Context

ArtForge is a Windows-first modular creative project system. The initial UI
needs separate, predictable desktop shells for solution, artist, series, and
work scopes. It should feel like a practical desktop tool with native windows,
menus, toolbars, tree views, list views, tables, split panes, tabs, dialogs, and
status bars.

The goal is not literal Windows XP compatibility. The goal is an XP-era
tool-like interaction model: plain, inspectable, fast, and conventional.

## Decision

Use the native Win32 API and Windows Common Controls as the initial GUI stack.

Create a small `ArtForgeUiWin32` wrapper layer in a later implementation task to
own repeated window registration, message dispatch helpers, control creation,
layout utilities, command routing, diagnostics panes, and localization-key
plumbing.

Use Direct2D later for custom views that need drawing beyond standard controls,
such as dependency graphs, canvas views, pressure maps, and mind maps.

## Excluded Initial Main UI Stacks

The initial main UI will not use:

- Ultimate++
- MFC
- wxWidgets
- Qt
- Dear ImGui

MFC is not selected for the initial pass because the project should first keep
the UI boundary explicit and small. wxWidgets or Qt can be reconsidered only if
real cross-platform requirements appear. Dear ImGui remains unsuitable as the
main application UI because ArtForge needs conventional desktop windows,
controls, dialogs, and accessibility expectations.

## Consequences

- The Visual Studio solution remains a modular C++ solution.
- Early shell work should depend only on Windows SDK headers and libraries that
  ship with the toolchain.
- Standard controls should be preferred for tree/list/table/tab/menu/status UI.
- Custom rendering should be isolated to future Direct2D-backed views.
- UI text remains English by default.
- Finnish translations remain separate under `locales/` when UI keys are added
  or changed.
- This decision documents direction only; it does not implement the Win32
  wrapper or a full GUI editor.
