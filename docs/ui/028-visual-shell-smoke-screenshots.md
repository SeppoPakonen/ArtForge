# Visual Shell Smoke Screenshots

This checklist validates ArtForge shell credibility without requiring
pixel-perfect automated GUI tests or adding repeated binary screenshot churn.

## Existing Mockup References

The repository already keeps visual mockups under `docs/mockup/`:

- `docs/mockup/solution_v1.png`
- `docs/mockup/artist_v1.png`
- `docs/mockup/series_v1.png`
- `docs/mockup/work-lyrics_v1.png`
- `docs/mockup/work-fineart_v1.png`
- `docs/mockup/work-dialogue_v1.png`

Use those files as reference material for shell rhythm and scope-specific
organization. New screenshots should only be committed when they document a
meaningful design milestone.

## Capture Setup

Build first:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' ArtForge.sln /t:Clean,Build /m /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145 /verbosity:minimal
```

Launch these smoke cases:

```powershell
.\bin\Debug\x64\ArtForgeSolutionApp.exe examples\graph\sample.afsolution.json
.\bin\Debug\x64\ArtForgeArtistApp.exe examples\graph\artists\sample.afartist.json
.\bin\Debug\x64\ArtForgeSeriesApp.exe examples\graph\projects\sample-series\sample.afseries.json
.\bin\Debug\x64\ArtForgeWorkApp.exe examples\work-domains\lyrics.afwork.json
```

Also launch each executable without arguments to verify the no-file empty state.

## Visual Smoke Criteria

Each app passes the shell smoke when the first window shows:

- Menu and shared command bar.
- Left navigation explorer with intentional roots.
- Central document tabs with Start and Current Scope rhythm.
- Right inspector with sectioned properties.
- Bottom panel with Output, Tasks, Provider, and History tabs.
- Status bar with a concise current state.
- Consistent Windows GUI font, spacing, and pane margins.
- No overlapping controls at the default window size.
- No raw static debug dump as the primary center experience.

## Scope Checklist

### Solution App

- Navigation root reads as a solution explorer.
- Artists, series/projects, works, and diagnostics are grouped.
- Start/current scope area shows the opened solution path and load status.
- Bottom panel includes load output and history/provider placeholders.

### Artist App

- Empty state includes Current Scope, Open Recent, Examples, and Diagnostics.
- Loaded state groups artist identity and projects.
- Inspector separates Scope, Selection, File, Provider, and Diagnostics.

### Series App

- Loaded state groups parent artist and ordered works.
- Start/current scope tabs are visible.
- Bottom panel is present even when there are no active tasks.

### Work App

- Work domain table remains visible inside the central document shell.
- Prompt/suggestion/provider labels are integrated into tabs and panels.
- Selecting a work row updates inspector and command availability.

## Before / After Notes

Before this UI shell credibility pass, the apps primarily looked like a debug
Win32 layout: left tree, static center text, raw right details list, and status
bar. The target baseline after tasks `021` through `027` is a native Win32 shell
with a visible command surface, document tab rhythm, intentional explorer,
sectioned inspector, bottom output/task panel, and status bar.

Docking, floating panes, and custom canvas rendering remain out of scope.
