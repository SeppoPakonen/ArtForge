# Loaded Scope Document Tabs

Task: `plan/05-ui-shells/033-add-loaded-scope-document-tabs.md`

The shared shell rebuilds center document tabs from the current scope and load
state:

- Solution: `Overview`, `Scope Graph`, `Diagnostics`
- Artist: `Overview`, `Artist Profile`, `Works Overview`, `Diagnostics`
- Series: `Overview`, `Series Overview`, `Ordered Works`, `Diagnostics`
- Work: `Overview`, `Work Domain`, `Prompt Preview`, `Suggestion Review`,
  `Diagnostics`

When no file is loaded, the first tab remains `Start` and the center list shows
Recent/Examples actions. Loaded tabs are read-only at this stage; editing stays
behind existing command-service routes.

## Manual Smoke Checks

Launch each scope app with its example file and confirm the center tab labels
match the scope. Refresh and Open should rebuild the tabs without duplicating
items.
