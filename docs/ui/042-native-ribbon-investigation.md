# Native Ribbon Investigation

This task investigates Ribbon UI as a later option. It does not add Ribbon
resources, generated markup, command handlers, or runtime dependencies.

## Current Command Strip

The current shell uses a small shared Win32 command strip in `ArtForgeUiWin32`.
It is grouped into File, AI, and Suggestion sections, and command availability
is driven by shell/presentation state.

Strengths:

- simple C++ implementation
- shared by all four scope apps
- no generated resource pipeline
- easy to keep command enablement bound to presentation models
- suitable for the current developer-oriented phase
- low localization surface

Costs:

- visually modest
- no built-in large/small command layout
- no built-in keyboard tip model
- icons and richer layout would need more wrapper work

## Native Windows Ribbon Framework

Native Ribbon could provide a more familiar command surface for a future
polished desktop application, but it has a larger resource and command-binding
footprint than the current shell needs.

Potential benefits:

- structured tabs, groups, and command presentation
- standard Windows command-surface behavior
- better fit if WorkApp grows many editing, AI, review, and export commands
- possible future alignment with Office-like productivity expectations

Costs and risks:

- requires Ribbon markup/resources and build integration
- icon sets become mandatory for a credible result
- command state binding needs a separate adapter from presentation models to
  Ribbon command properties
- localization becomes broader because labels, tooltips, keytips, and resources
  must be managed together
- DPI and density behavior must be validated separately from current Common
  Controls panes
- four scope apps may not need enough commands to justify the complexity yet
- Ribbon would not solve splitter, layout, document, or editing workflow issues

## Future Custom Command Surface

A custom native command surface could stay close to the current architecture
while supporting richer grouping, icons, and context-sensitive rows.

Potential benefits:

- keeps command binding explicit and testable
- can evolve gradually from the existing wrapper
- can remain small for Solution, Artist, and Series apps while WorkApp grows
- can keep localization keys in ordinary source/JSON rather than Ribbon
  resource files

Costs and risks:

- owner-drawn or custom layout code increases visual and accessibility burden
- keyboard navigation must be implemented deliberately
- visual polish can drift without a clear design system
- still requires a proper icon and DPI strategy

## Recommendation

Defer Ribbon. Revisit after these conditions are true:

- WorkApp has enough stable editing, AI, suggestion review, and export commands
  to need multiple command tabs
- the native shell redraw/layout path has stayed stable across several UI tasks
- localization keys and command descriptions are centrally modeled
- a DPI-scaled icon/resource strategy exists
- user workflows show that the current grouped command strip is too limited

Until then, continue improving the shared command strip and presentation-driven
command model. That keeps the shell simpler while the domain model and editing
flows are still changing.

