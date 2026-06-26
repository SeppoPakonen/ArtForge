# ArtForge

ArtForge is a modular creative project system for modeling artist identity,
creative constraints, projects, albums, series, works, songs, images, scenes,
fragments, sources, rule packages, AI prompt packages, persistent history, and
dependency or conflict analysis.

The current phase is developer-oriented. The repository is organized as a
Visual Studio 2022 C++ solution with small modules for core types, files,
history, prompting, dependency modeling, and placeholder scope-specific
applications.

Application UI text is English by default. A path for future Finnish UI
translations is maintained under `locales/`, starting with `locales/ui.fi.json`,
but full localization is not implemented yet.

The first implementation favors human-readable, git-friendly project data and
module boundaries. Future Windows Store packaging or a simplified bundled
single-file product format is intentionally out of scope for this phase.

The initial GUI direction is native Win32 API plus Windows Common Controls, with
Direct2D reserved for later custom graph, canvas, pressure-map, and mind-map
views. See [docs/architecture/002-gui-stack.md](docs/architecture/002-gui-stack.md).
The shared `ArtForgeUiWin32` module now provides the minimal native shell
foundation used by the scope applications.

## UI mockups

These mockups show the intended visible direction for the modular scope shells.
They are references for future UI work, not implemented editor screens yet.

### Solution scope

![ArtForge solution scope mockup](https://github.com/SeppoPakonen/ArtForge/blob/main/docs/mockup/solution_v1.png?raw=true)

### Artist scope

![ArtForge artist scope mockup](https://github.com/SeppoPakonen/ArtForge/blob/main/docs/mockup/artist_v1.png?raw=true)

### Series scope

![ArtForge series scope mockup](https://github.com/SeppoPakonen/ArtForge/blob/main/docs/mockup/series_v1.png?raw=true)

### Work scope: lyrics

![ArtForge lyrics work scope mockup](https://github.com/SeppoPakonen/ArtForge/blob/main/docs/mockup/work-lyrics_v1.png?raw=true)

### Work scope: visual art

![ArtForge visual art work scope mockup](https://github.com/SeppoPakonen/ArtForge/blob/main/docs/mockup/work-fineart_v1.png?raw=true)

### Work scope: dialogue

![ArtForge dialogue work scope mockup](https://github.com/SeppoPakonen/ArtForge/blob/main/docs/mockup/work-dialogue_v1.png?raw=true)

## Process model

ArtForge keeps the main creative scopes independently launchable:

- `ArtForgeSolutionApp.exe <solution-file>`
- `ArtForgeArtistApp.exe <artist-file>`
- `ArtForgeSeriesApp.exe <series-file>`
- `ArtForgeWorkApp.exe <work-file>`

Shared process-model definitions live in `ArtForgeCore`. The current boundary is
only a stub for future cross-scope messaging; it names the event types the
modules are expected to exchange later, including file changes, snapshots,
dependency graph invalidation, rule and prompt package changes, AI imports,
conflicts, and status updates. It is not an IPC system yet.

## File model

ArtForge project data is modular and human-readable. Structured project files
use JSON, append-only history uses JSON Lines, and human-authored rules or
explanations use Markdown. Portable project references should be relative paths
unless a field is explicitly machine-local metadata. Generated caches should be
kept outside authored project files, under a cache directory such as
`.artforge-cache`.

Initial file roles are represented in `ArtForgeFiles`:

- `*.afsolution` for solution files
- `*.afartist` for artist profile files
- `*.afseries` for project, album, or series files
- `*.afwork` for work item files
- `*.affragment` for fragment or source files
- `*.afhistory` or `*.afhistory.jsonl` for persistent history
- `*.afrules` for rule package manifests
- `*.afprompt` for AI prompt package manifests

Minimal scope file schemas are documented under `docs/file-formats/`, with
sample JSON files under `examples/minimal/`.
The persistent history JSON Lines format is documented in
`docs/file-formats/afhistory.md`.
`ArtForgeFiles` provides minimal load/save helpers for the solution, artist,
series, and work scope JSON files. These helpers check that child references are
relative paths. It also provides a small read-only project graph loader that
links solution, artist, series, and work files through relative references and
reports missing, invalid, or unsupported nodes without throwing. A nested graph
sample lives under `examples/graph/`.
`ArtForgeHistory` provides minimal append/read helpers for `.afhistory.jsonl`
events. Malformed lines are reported as diagnostics while valid events remain
readable. Snapshot metadata and branch placeholders are represented as ordinary
history events for now; full undo/redo restoration and branch switching remain
out of scope.

One expected project folder shape is:

```text
MyArtist/
  artist.afartist
  rules/
    general.md
    artist.md
    ui.fi.json
  projects/
    FirstAlbum/
      series.afseries
      works/
        01-opening/
          work.afwork
          sources/
          history.afhistory.jsonl
```

Finnish UI translation remains a separate repository-level concern under
`locales/ui.fi.json`. A later bundled single-file consumer format may be added as
a packaging layer, but it is not the primary project format in this phase.

## History model

ArtForge history is separate from Git. Git remains the repository checkpoint and
collaboration tool; ArtForge history records higher-frequency creative
operations that can later support browse, undo, redo, compare, and branch
workflows.

The initial storage convention is append-only JSON Lines plus optional JSON
snapshots:

```text
history.afhistory.jsonl
snapshots/
  000001.json
  000002.json
```

Each planned history item includes a stable id, timestamp, actor, scope,
affected files, operation type, summary, optional before and after references,
optional prompt package id, and optional AI result id. The first operation
surface is intentionally small: create a history item, list history items,
restore a snapshot placeholder, and branch metadata placeholder.

Example operation types include user text edits, accepted AI repair options,
rejected AI suggestions, imported source fragments, rule weight changes, prompt
package generation, and AI JSON result imports. Branching is represented as
metadata for now; no optimized history engine or branch UI is implemented yet.

## Prompt package model

ArtForge prompt packages are layered context bundles, not ad-hoc chat text. The
initial contract is represented in `ArtForgePrompting` and keeps
human-authored rules in Markdown while technical state and output contracts stay
in JSON.

Prompt context is ordered as:

1. General creative rules: `general_rules.md`
2. Domain rules: `domain_rules.md`
3. Artist or subject rules: `artist_rules.md`
4. Project or series rules: `project_rules.md`
5. Work item state: `current_state.json`
6. User markings and repair requests: `task_instruction.md`
7. Output contract: `output_schema.json`

`CreativeSubjectProfile` is planned as a domain-neutral abstraction. In lyrics
it may describe a singer or vocal character; in visual art it may describe the
main subject, figure, object, or viewpoint; in story or game work it may
describe the narrator, protagonist, or player-facing role.

AI output must be structured JSON before import. The placeholder output schema
concepts are suggestions, critique items, replacement candidates, rule hits,
rule violations, unresolved questions, confidence, and rationale. Free-form AI
text is not imported directly into project state.

Before any AI result changes state, ArtForge will parse JSON, validate the
schema, verify referenced ids, reject unknown destructive operations, create a
history item, and ask the user to accept or reject suggested changes. Prompt
package generation maps to the history operation `prompt package generated`;
validated AI result import maps to `AI JSON result imported`.
The concrete prompt package schema is documented in
[docs/prompting/prompt-package-schema.md](docs/prompting/prompt-package-schema.md),
with a small layered example under `examples/prompt-package/work-repair/`.
`ArtForgePrompting` can now assemble a deterministic prompt package debug dump
from loaded work context and layered Markdown/JSON files. The example inputs
live under `examples/prompt-build/`; no AI provider is called.

## Scope shell model

The first UI phase is a set of independently launchable placeholder shells, not
a full editor. `ArtForgeCore` owns shared shell descriptors for:

- `ArtForgeSolutionApp`: solution scope
- `ArtForgeArtistApp`: artist scope
- `ArtForgeSeriesApp`: project, album, or series scope
- `ArtForgeWorkApp`: work item scope

Each executable opens a minimal native Win32 window showing its application
name, expected scope, optional path argument, and `ArtForge bootstrap OK`.
The Win32 shell also includes a simple left navigation tree that shows the
current scope, opened path, load status, and parent or child references when
available from the loaded scope file or solution graph.
The shared UI module also contains a small reusable pane layout helper for a
left navigation pane, center workspace pane, right detail/action pane, and
bottom status area. This is not a docking system.
`ArtForgeWorkApp` detects the optional `workDomain` field in work files and
shows placeholder workspace text for lyrics/music, visual art, and
script/storyboard domains. Unsupported domains produce an explicit placeholder
status.

The shell model also preserves future navigation concepts without implementing
them as GUI views yet. Solution shells reserve artists, projects, missing files,
world update view, conflicts, and tasks. Artist shells reserve identity map,
recurring themes, constraints, audience assumptions, and projects. Series shells
reserve ordered works, arc pressure, missing slots, requirements, and conflicts.
Work shells reserve technical base, sources, editor area, pressure map, rule
analytics, repair options, and history timeline.

Future UI labels should use stable localization keys. English remains the
default UI language, with Finnish translations maintained separately under
`locales/`.

## Creative pressure model

`ArtForgeDeps` defines a Portage-inspired pressure model for creative work. It
is not a package manager clone and does not include a solver yet. The model uses
dependency terminology to make incompatible creative goals visible before trying
to resolve them.

Core terms:

- Package: selected creative requirement, work, rule pack, fragment, or project
  function
- Flag: user choice that changes package behavior or requirements
- Slot: role that may need one or more compatible installed items
- Version: specific revision of a package or creative requirement
- Dependency: item required by another item
- Blocker: item that prevents another item from being valid
- Circular dependency: requirement cycle needing a changed decision
- World set: selected creative goals and installed requirements

Example flag usage:

```text
package: lyrics-low-friction-policy
flags:
  non_native_listener
  radio_target
  2010_language
  male_performer
```

Creative examples include live solo performance requirements, low-cost music
video requirements, and project or album story-arc requirements. These examples
are modeled as creative pressures such as singable range, portable arrangement,
few locations, daylight shooting, opening contrast, midpoint escalation, and
closing resolution.

Initial diagnostic categories are missing dependency, missing slot, blocker,
flag conflict, circular dependency, version mismatch, and unresolved pressure.
They are intended for later UI display and history recording, not automatic
resolution in this phase.

Minimal package data types now live in `ArtForgeDeps`, including package ids,
versions, flags, slots, dependencies, blockers, diagnostic severities, and
diagnostic messages. See
[docs/dependency-model/pressure-packages.md](docs/dependency-model/pressure-packages.md)
for the first documented pressure package examples.

A future world update view should show packages needing update, missing slots,
unresolved conflicts, fragments not imported, tasks required before a version
can be accepted, and works blocked by project-level decisions.
`ArtForgeDeps` now includes a basic world update summary formatter that groups
current diagnostics for console output and later UI display; it does not run a
solver.
