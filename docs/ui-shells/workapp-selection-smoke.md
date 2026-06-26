# WorkApp Selection Smoke Notes

WorkApp row selection is UI-neutral after the presentation adapter boundary.
The Win32 list view sends only the selected row index; the presentation adapter
maps that row to a `SelectionModel` and refreshes the property panel model.

Manual smoke coverage:

- `ArtForgeWorkApp.exe examples/work-domains/lyrics.afwork.json`: select the
  first lyric row. The property panel should show selected domain `lyrics`,
  item type `lyricLine`, a line id, index `0`, and the line text as the
  selected item label.
- `ArtForgeWorkApp.exe examples/work-domains/visual-art.afwork.json`: select
  the first layer row. The property panel should show selected domain
  `visualArt`, item type `visualLayer`, a layer id, index `0`, and the layer
  label.
- `ArtForgeWorkApp.exe examples/work-domains/script-storyboard.afwork.json`:
  select the first block or scene row. The property panel should show selected
  domain `scriptStoryboard`, item type `scriptScene` or `scriptBlock`, the item
  id, row index, and a display label.
