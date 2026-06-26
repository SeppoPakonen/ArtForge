# AI Result JSON Contract And Pending Suggestions

AI provider output is never applied directly to ArtForge project files. Import
starts from JSON validation and creates pending suggestions that must later be
accepted or rejected.

## Top-Level Shape

```json
{
  "format": "artforge.ai.selectedItemResult",
  "schemaVersion": 1,
  "resultId": "ai.result.example",
  "promptPackageId": "prompt.package.example",
  "target": {
    "workPath": "examples/work-domains/lyrics.afwork.json",
    "domain": "lyrics",
    "itemType": "lyricLine",
    "itemId": "line.v1.001",
    "itemIndex": 0,
    "field": "text"
  },
  "suggestions": [],
  "critiqueItems": [],
  "replacementCandidates": [],
  "ruleHits": [],
  "ruleViolations": [],
  "unresolvedQuestions": [],
  "confidence": 0.74,
  "rationale": "Short explanation for review."
}
```

Required sections are `suggestions`, `critiqueItems`,
`replacementCandidates`, `ruleHits`, `ruleViolations`,
`unresolvedQuestions`, `confidence`, and `rationale`.

## Replacement Candidate

Replacement candidates propose a single selected field edit. The first minimal
candidate shape is:

```json
{
  "id": "candidate.001",
  "operation": "replaceText",
  "field": "text",
  "proposedText": "Replacement text for review.",
  "rationale": "Why this candidate may help.",
  "diagnostics": []
}
```

Unknown destructive operations must be rejected. Free-form text outside the JSON
contract is ignored.

## Pending Suggestion

After validation, ArtForge may represent a candidate as a pending suggestion:

- source prompt package id and optional path
- target work path, domain, item type, item id/index, and field
- proposed text
- rationale and diagnostics
- status: `pending`, `accepted`, or `rejected`

Pending suggestions are review state. They are not applied changes. Applying a
suggestion must go through edit command, change-set, history, and save logic.

## Local Import

The initial import path reads a local AI result JSON file, validates required
sections, checks that the result target matches the expected selected work item,
and appends pending suggestions to `pending-suggestions.jsonl` beside the work
file. The import path records reviewable pending data only; it does not edit or
save the work file.
