# Common Controls Font and Spacing Baseline

ArtForge scope apps use native Win32 and Windows Common Controls through
`ArtForgeUiWin32`. The shell baseline now centralizes common-control
initialization, visual-style readiness, default GUI font application, and layout
metrics.

## Baseline

- `InitializeCommonControlsForShell` initializes bar, standard, tree, list, and
  tab common-control classes.
- The shared shell carries a Common Controls v6 manifest dependency.
- `ApplyDefaultGuiFont` applies the Windows message font to tree, list, tab,
  static, status, and property controls created through shared wrappers.
- `DefaultShellUiMetrics` defines shared spacing values for margin, gap,
  splitter width, toolbar height, tab height, inspector row height, output pane
  height, navigation width, inspector width, and minimum document width.
- The three-pane layout reads its sizes from `ShellUiMetrics` instead of local
  magic numbers.

## Manual Smoke Notes

Launch each app with and without a matching sample scope file:

- `ArtForgeSolutionApp.exe examples\graph\sample.afsolution.json`
- `ArtForgeArtistApp.exe examples\graph\artists\sample.afartist.json`
- `ArtForgeSeriesApp.exe examples\graph\projects\sample-series\sample.afseries.json`
- `ArtForgeWorkApp.exe examples\work-domains\lyrics.afwork.json`

Expected result:

- Controls use a consistent Windows GUI font.
- Left navigation, central content, right inspector, and status bar have
  consistent margins and gaps.
- Common Controls visual style is available when the executable manifest honors
  the shared dependency.
- The WorkApp table and property panel keep their existing behavior.
