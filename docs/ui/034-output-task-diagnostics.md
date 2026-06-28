# Output And Task Diagnostics

Task: `plan/05-ui-shells/034-populate-output-and-task-panel-from-diagnostics.md`

The bottom panel now renders read-only diagnostic rows with:

- `Severity`
- `Area`
- `Message`

Rows are populated from current shell/service state:

- Load success or failure.
- Actionable no-file and load-failure tasks.
- WorkApp manual queue diagnostics.
- Provider configuration/request status.
- History status hints.

Common severities are `Info`, `Warning`, and `Error`.

## Manual Smoke Checks

1. Launch without a path and confirm the task row suggests opening a file.
2. Launch with a valid scope file and confirm load rows are `Info`.
3. Launch with an incompatible or invalid file and confirm load rows are `Error`.
4. Launch WorkApp and confirm provider/manual queue rows appear with warning
   severity when provider configuration is missing.
