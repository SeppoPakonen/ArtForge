# Pending Suggestion Review Architecture

ArtForge treats AI output as proposed input, not as an edit. The review
workflow is intentionally explicit:

```text
AI/manual/API result JSON
  -> validated AI result
  -> pending suggestion
  -> review state
  -> accept or reject command
  -> edit command/change set when accepted
  -> history events
```

Suggestions must never be applied automatically. Importing AI result JSON can
only create pending suggestions. Work files change only after the user accepts a
suggestion, or after an equivalent explicit command is run.

## Boundaries

- `ArtForgePrompting` owns provider-neutral AI result validation, pending
  suggestion records, and review-oriented metadata.
- `ArtForgeServices` owns accept/reject commands and converts accepted
  suggestions into edit commands/change sets.
- `ArtForgeHistory` records review lifecycle events and change-set events.
- `ArtForgePresentation` builds UI-neutral review and compare models.
- `ArtForgeUiWin32` displays presentation models and forwards commands. It does
  not interpret domain files directly.

This follows the presentation adapter boundary from
`docs/architecture/004-presentation-adapter-architecture.md` and the edit
command/change-set boundary from
`docs/architecture/006-edit-command-and-change-set-boundaries.md`.

## Terms

- Pending suggestion: a validated AI result item stored for review. It contains
  an id, prompt package reference, target metadata, proposed text, rationale,
  diagnostics, and status.
- Suggestion target: work path/id, domain, item type, item id/index, and editable
  field. The target must still match before apply.
- Suggestion status: `pending`, `accepted`, or `rejected`. Future states may add
  more detail, but rejected suggestions are not eligible for normal apply.
- Review state: UI-neutral data that shows the target, original/current text,
  proposed text, rationale, diagnostics, and available commands.
- Accept command: validates that the pending suggestion is still applicable,
  then creates an edit command/change set.
- Reject command: records that the suggestion was intentionally declined without
  modifying work content.

## Accept Flow

1. Load pending suggestion from `pending-suggestions.jsonl` or equivalent
   service input.
2. Validate status is `pending`.
3. Validate target metadata against the opened work file and current selected
   item.
4. Compare expected original/current text when available.
5. Create a replace-text edit command for the target field.
6. Apply through the edit command service and safe work-domain file roundtrip.
7. Record suggestion apply request/success/failure and change-set history.
8. Mark the suggestion accepted/applied in review output where supported.

The accept path must not write around the edit command service.

## Reject Flow

1. Load pending suggestion.
2. Validate status is `pending`.
3. Record rejection reason when provided.
4. Mark the suggestion as rejected in review output where supported.
5. Record suggestion rejected history.
6. Leave work files unchanged.

## Examples

Lyric line replacement:

- target: `lyrics/lyricLine#line.v1.001.text`
- original/current: `I leave the porch light on tonight`
- suggestion: `I keep the doorway lit tonight`
- apply: replace the lyric line text through the selected text edit command.

Visual art layer intent refinement:

- target: `visualArt/visualLayer#viewer.foreground.intent`
- original/current: `Foreground establishes scale.`
- suggestion: `Lead the eye to the figure before the background.`
- apply: replace the layer intent through the same edit/change-set path.

Script/storyboard block rewrite:

- target: `scriptStoryboard/scriptBlock#block.opening.dialogue.text`
- original/current: `We start before anyone knows what changed.`
- suggestion: `We start before the room understands what changed.`
- apply: replace the script block text through the edit command service.

## History

Suggestion review events should describe:

- suggestion imported
- review opened
- accepted
- rejected
- apply requested
- apply succeeded
- apply failed

Each event should carry suggestion id, source request/result path, target
metadata, status, and a short diagnostic summary. History recording is
non-fatal; failure to append history must not apply or reject a suggestion by
itself.

## Out Of Scope

- real API calls
- automatic merge
- multi-suggestion conflict resolution
- automatic application of suggestions
- full undo/redo restoration
