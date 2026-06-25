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
