# Recent And Example Start Page Actions

Task: `plan/05-ui-shells/032-add-recent-and-example-start-page-actions.md`

The center pane now shows a list-based start surface when no file is open.

Sections:

- `Open`: prompt to use the Open command.
- `Recent`: compatible entries from `%APPDATA%\ArtForge\settings\recent-files.json`.
- `Example`: compatible checked-in examples from `examples/`.

Double-clicking a Recent or Example row loads through the same service path as
the Open command. Incompatible entries are filtered per scope app. Missing recent
files are diagnosed by the recent-file service and not shown as openable rows.

## Manual Smoke Checks

For each scope app:

1. Launch without a file path.
2. Confirm the center pane shows `Open` and compatible `Example` rows.
3. Open a valid file, close the app, and relaunch without a path.
4. Confirm the matching recent row appears.
5. Double-click a recent or example row and confirm the app loads it.
