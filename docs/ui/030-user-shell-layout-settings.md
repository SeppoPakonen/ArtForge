# User Shell Layout Settings

Task: `plan/05-ui-shells/030-persist-user-window-and-pane-layout.md`

ArtForge stores Win32 shell layout in a user-local JSON file:

```text
%APPDATA%\ArtForge\settings\shell-layout-<application>.json
```

The settings file is outside project directories, examples, and git-tracked
source files. It is safe to delete; the shell falls back to default window and
pane sizes when the file is missing or invalid.

Persisted fields:

- Normal window position and size.
- Maximized state.
- Explorer pane width.
- Inspector pane width.
- Bottom panel height.
- Selected bottom panel tab.

## Reset Behavior

Close the application, delete the relevant `shell-layout-*.json` file under
`%APPDATA%\ArtForge\settings`, and launch the app again. Defaults are recreated
after the next clean close.

## Manual Smoke Checks

For each scope app:

1. Launch the app.
2. Resize the window and drag the explorer, inspector, and bottom splitters.
3. Select a non-default bottom tab.
4. Close and relaunch the app.
5. Confirm the window geometry, pane sizes, and bottom tab are restored.
6. Corrupt or delete the settings file and confirm the app starts with defaults.
