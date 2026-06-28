# Scoped Open File Command

Task: `plan/05-ui-shells/031-add-open-file-command-per-scope.md`

The shared Win32 shell uses the native Windows Open dialog for the toolbar and
File menu `Open` command. Filters are scope-specific:

- Solution: `*.afsolution.json`
- Artist: `*.afartist.json`
- Series: `*.afseries.json`
- Work: `*.afwork.json`

After a successful load, the shell refreshes navigation, document content,
inspector, output/status, and recent files. Canceling the dialog leaves the
current shell state unchanged. Parse and validation failures are surfaced through
the existing load status and diagnostics surfaces.

## Manual Smoke Checks

For each app:

1. Launch without a path.
2. Use `Open` from the command bar.
3. Select a valid matching example file.
4. Confirm the explorer, document area, inspector, status bar, and bottom panel
   update.
5. Reopen and cancel the dialog; confirm the loaded state is unchanged.
6. Try an incompatible or invalid JSON file; confirm the failure is visible and
   the app does not mutate the file.
