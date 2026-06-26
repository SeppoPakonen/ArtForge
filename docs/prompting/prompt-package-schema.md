# ArtForge Prompt Package Schema

## Purpose

An ArtForge prompt package is a frozen, reviewable context bundle for one AI
request. It is not a chat transcript. The package keeps human-authored creative
rules in Markdown and technical state, references, and output contracts in JSON.

ArtForge does not call an AI service or import AI output in this phase. Any AI
output produced later must be JSON, must validate against the package output
contract, and must be accepted by the user before it can change project state.

## Package Manifest

Recommended extension: `.afprompt.json`

```json
{
  "format": "artforge.promptPackage",
  "schemaVersion": 1,
  "id": "prompt.sample.work.repair.0001",
  "createdUtc": "2026-06-25T00:00:00Z",
  "scope": {
    "type": "work",
    "id": "work.opening-song",
    "path": "../minimal/sample.afwork.json"
  },
  "layers": [
    {
      "kind": "general creative rules",
      "format": "Markdown",
      "path": "general_rules.md"
    }
  ],
  "output": {
    "format": "JSON",
    "schemaPath": "output_schema.json",
    "requiresValidationBeforeImport": true
  }
}
```

## Required Layers

Layers are evaluated in this order:

1. `general_rules.md`: broad creative policy
2. `domain_rules.md`: domain-specific rules
3. `artist_rules.md`: artist, subject, narrator, or viewpoint rules
4. `project_rules.md`: project, album, or series rules
5. `current_state.json`: current technical state and stable ids
6. `task_instruction.md`: user markings and requested repair
7. `output_schema.json`: required JSON result contract

Markdown layers are written for human review. JSON layers are written for
validation and deterministic import checks. Paths in the manifest should be
relative to the prompt package directory.

## Current State JSON

`current_state.json` is a snapshot of the minimum technical context the AI needs
for the request. It should include stable ids instead of relying only on display
text.

Required top-level fields:

- `format`: `artforge.prompt.currentState`
- `schemaVersion`: positive integer
- `scope`: current scope type, id, display name, and source file path
- `focus`: selected fragment, range, or work area under repair
- `knownReferences`: referenced files or ids that AI output may refer to

## Output Contract

AI output must be one JSON object. Free-form prose is not imported directly.

The output contract should require these sections:

- `suggestions`
- `critiqueItems`
- `replacementCandidates`
- `ruleHits`
- `ruleViolations`
- `unresolvedQuestions`
- `confidence`
- `rationale`

Before import, ArtForge must parse the JSON, validate the schema, verify
referenced ids, reject unknown destructive operations, create a history item,
and ask the user to accept or reject the result.

## Out Of Scope

This schema does not implement an AI provider, prompt execution, automatic state
import, undo/redo restoration, or a prompt browser UI.

## Domain Examples

Layered prompt package examples for lyrics/music, visual art layers, and
script/storyboard work live under `examples/prompt-domains/`. Each example uses
the same seven-layer package structure and keeps AI output constrained to JSON
that must be validated before import.
