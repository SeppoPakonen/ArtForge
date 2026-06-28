# Command Bar States

Task: `plan/05-ui-shells/035-bind-command-bar-states-to-presentation-model.md`

Toolbar and File menu availability now come from the same command state model.

State matrix:

- `Open`: enabled.
- `Refresh`: enabled.
- `Save`: enabled only for dirty WorkApp state with `canSave`.
- `Build Prompt`: enabled only when a work item row is selected and prompt
  preview is available.
- `Queue Manual AI Task`: enabled only when a work item row is selected and the
  manual queue model is available.
- `Accept Suggestion`: enabled only when the pending suggestion review model
  exposes an enabled accept command.
- `Reject Suggestion`: enabled only when the pending suggestion review model
  exposes an enabled reject command.

Disabled reasons are tracked in the command state and can be surfaced in richer
tooltips/status help later. The current native button wrapper applies enabled
and visible state; no command bypasses service or presentation routes.

## Manual Smoke Checks

1. Launch with no file: Open and Refresh are enabled; work commands are disabled.
2. Launch WorkApp with a valid work file: Save remains disabled while clean.
3. Select a work-domain row: Build Prompt and Queue Manual AI Task become
   enabled.
4. Launch non-work apps: Work-only commands remain disabled.
