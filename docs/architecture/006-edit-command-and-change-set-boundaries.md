# Edit Command And Change-Set Boundaries

ArtForge editing must be command-driven. Views report user intent; they do not
mutate loaded file data, domain models, prompt packages, or history records
directly.

The required edit flow is:

```text
View -> Presentation Adapter -> Application Service -> Model/File -> History -> Presentation Model
```

The Win32 shell remains a rendering and input layer. It may read presentation
models, populate controls, collect text from controls, and send edit requests.
It must not interpret work-domain schemas or write changed values into loaded
domain/file structures.

This document extends the presentation adapter architecture in
[004-presentation-adapter-architecture.md](004-presentation-adapter-architecture.md).

## Terms

### Edit Command

An edit command is a small, explicit request to change authored state. It names
the command target, intended operation, proposed value, actor, and source of the
request.

Examples:

- replace lyric line text
- update visual layer intent
- replace script block dialogue or action text
- accept a pending AI suggestion

Commands are user or automation intent. They are not persistence records and are
not allowed to skip validation.

### Command Target

A command target identifies the authored item being edited without requiring the
view to understand the domain object. A target should include:

- scope path, normally a work file path for WorkApp editing
- domain id, such as `lyrics`, `visual_art`, or `script_storyboard`
- item type, such as `lyric_line`, `visual_layer`, or `script_block`
- stable item id when available
- fallback row/index only when a schema lacks stable ids
- field name, such as `text`, `intent`, `dialogue`, or `action`

Targets must be resolved and validated by an application service before any
model or file data is changed.

### Command Validation

Command validation checks whether the requested edit can be applied. It should
verify:

- the scope file can be loaded
- the requested domain is supported
- the item target exists
- the field is editable
- the new value is valid for the field
- the edit is not stale against the loaded model version when versioning exists
- the resulting file can still be serialized in the human-readable format

Validation failures return diagnostics to presentation models. They do not
partially mutate state.

### Change Set

A change set is the validated, model-facing representation of one command or a
small atomic group of commands. It is the unit that can be applied, reported as
dirty, saved, and recorded in history.

A change set should include:

- a stable change set id
- originating command id
- actor and timestamp
- target path and domain
- one or more pending changes
- validation diagnostics
- save status
- optional history event id after recording

Single-field edits should still use change sets so the same path can later
support grouped edits and AI suggestion acceptance.

### Pending Change

A pending change is a validated edit that has not yet been committed to the
loaded model or file state. It contains enough information to show a preview,
including field name, before value, after value, and target identity.

Pending changes are useful for AI suggestions, preview panes, and future
multi-edit confirmation.

### Applied Change

An applied change has been written into the loaded in-memory model. Applied
changes make the scope dirty until a save transaction writes them to disk.

An applied change is not the same as a saved change. If save fails, the loaded
model may remain dirty and the presentation model must show that state.

### Dirty State

Dirty state means the loaded in-memory state differs from the last successful
save. Dirty state belongs to the application service or document/session model,
not to a Win32 control.

Presentation models may expose:

- `isDirty`
- `canSave`
- `lastSavedPath`
- `pendingChangeCount`
- `lastSaveError`

Views render these values and enable or disable commands from them.

### Save Transaction

A save transaction writes a dirty scope back to disk. It should be atomic from
the user's perspective:

1. serialize the current model into the human-readable file format
2. write to a temporary file next to the target when possible
3. preserve readable formatting and stable ordering
4. replace the target file after successful write
5. record save success or failure in history when appropriate
6. update dirty state and presentation models

Safe save roundtrip must preserve fields that ArtForge does not currently edit
unless a file-format task explicitly allows normalization.

### History Event Relationship

History records describe commands and applied change sets. They are not the
primary in-memory edit mechanism.

The expected relationship is:

```text
Edit Command -> validated Change Set -> Applied Change -> History Event -> Save Transaction
```

History events should capture command identity, target identity, summary,
before/after values or references, affected files, actor, and timestamp. Full
undo/redo restoration remains out of scope for this stage.

## Domain Examples

### Lyric Line Text

1. The Win32 list view selects lyric line `line.v1.001`.
2. The property panel sends an edit command for target field `text`.
3. The presentation adapter maps the selected presentation item to a command
   target.
4. The application service loads or uses the current work model, validates that
   `line.v1.001` exists and that `text` is editable.
5. A change set records the old lyric line and proposed replacement text.
6. The model applies the new text, the document becomes dirty, and a history
   event records the change.
7. The presentation model refreshes the row, property value, dirty indicator,
   and save command state.

### Visual Layer Intent

1. The visual layer row `viewer.foreground` is selected.
2. The view sends an edit command for field `intent`.
3. The service validates that the current work domain is visual art and that the
   layer target exists.
4. The change set records the previous intent and the new intent text.
5. The applied change updates only the layer intent, not layer ordering,
   priority, or status.
6. Presentation refresh shows the new intent and dirty state.

### Script Block Dialogue Or Action Text

1. A script block such as `block.opening.dialogue` is selected.
2. The editor sends a command for `dialogue` or `action` text, depending on the
   selected block type.
3. The service validates the block id, editable field, and non-destructive
   replacement.
4. The change set stores before and after text for the block field.
5. The model applies the edit and records a history event that references the
   script/storyboard domain and affected work file.

## Dependency Direction

Allowed dependencies:

- Win32 views depend on presentation and UI wrapper modules.
- Presentation adapters depend on application service request/result types and
  presentation model types.
- Application services depend on file, history, prompting, dependency, and core
  modules as needed.
- Model modules do not depend on Win32, console output, or view classes.

Not allowed:

- Win32 controls directly editing loaded domain or file structures.
- Presentation models including `HWND` or Windows Common Controls types.
- Application services depending on Win32 controls.
- History appends being used as a substitute for command validation.
- AI provider output being applied directly to authored files.

## Out Of Scope

The controlled editing foundation does not implement:

- collaborative editing
- merge conflict resolution
- full undo/redo state restoration
- automatic dependency solving
- AI provider calls
- direct import of free-form AI text into authored state
- a full rich text or canvas editor
