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
