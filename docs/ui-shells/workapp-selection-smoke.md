# WorkApp Selection Smoke Notes

WorkApp row selection is UI-neutral after the presentation adapter boundary.
The Win32 list view sends only the selected row index; the presentation adapter
maps that row to a `SelectionModel` and refreshes the property panel model.

Manual smoke coverage:

- `ArtForgeWorkApp.exe examples/work-domains/lyrics.afwork.json`: select the
  first lyric row. The property panel should show selected domain `lyrics`,
  item type `lyricLine`, section, time, line text, line id, and index `0`.
- `ArtForgeWorkApp.exe examples/work-domains/visual-art.afwork.json`: select
  the first layer row. The property panel should show selected domain
  `visualArt`, item type `visualLayer`, layer type, label, intent, priority,
  and status.
- `ArtForgeWorkApp.exe examples/work-domains/script-storyboard.afwork.json`:
  select the first block or scene row. The property panel should show selected
  domain `scriptStoryboard`, item type `scriptScene` or `scriptBlock`, scene or
  block id, time range, speaker/type, and a summary.

For any selected supported item, the right panel should also show prompt preview
properties: preview availability, selected item summary, operation placeholder,
prompt layer summaries, output schema reference, and diagnostics if the preview
cannot be built. No AI provider is called.
