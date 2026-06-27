# Manual AI Queue File Format

Manual AI queue mode lets ArtForge use an external AI chat or agent without API
credentials. ArtForge writes numbered request and prompt files into a
user-owned queue directory, then waits for a matching JSON result file.

The default queue root convention is:

```text
%USERPROFILE%\Documents\ArtForge\ai-queue
```

The queue root is configurable and should normally stay outside the project
repository because prompt text can contain creative project content.

## File Names

Files use a six-digit sequence and a safe mechanical slug:

```text
000001-lyrics-line-repair.request.json
000001-lyrics-line-repair.prompt.txt
000001-lyrics-line-repair.status.json
000001-lyrics-line-repair.result.json
```

The slug is derived from requested operation and work domain. It must not use
raw arbitrary user text.

## Request JSON

Request files are UTF-8 JSON:

```json
{
  "format": "artforge.manualAiQueue.request",
  "schemaVersion": 1,
  "id": "manual-000001-lyrics-line-repair",
  "sequence": 1,
  "createdAt": "2026-06-27T00:00:00Z",
  "providerKind": "manualQueue",
  "requestedOperation": "line-repair",
  "promptPackage": {
    "path": "examples/prompt-selected-items/lyrics-line-repair.afprompt.json",
    "summary": "Selected lyric line repair prompt"
  },
  "copyPastePromptPath": "000001-lyrics-line-repair.prompt.txt",
  "expectedResultPath": "000001-lyrics-line-repair.result.json",
  "statusPath": "000001-lyrics-line-repair.status.json",
  "resultSchemaPath": "docs/prompting/ai-result-contract.md",
  "target": {
    "workPath": "examples/work-domains/lyrics.afwork.json",
    "domain": "lyrics",
    "itemType": "lyricLine",
    "itemId": "line.v1.001",
    "field": "text"
  }
}
```

No API key, bearer token, password, or other credential field is valid in a
request file.

## Prompt Text

Prompt text files are plain UTF-8 text intended for copy-paste. They include the
assembled prompt layers and a final instruction to return only JSON matching the
AI result contract.

## Status JSON

Status files are UTF-8 JSON:

```json
{
  "format": "artforge.manualAiQueue.status",
  "schemaVersion": 1,
  "requestId": "manual-000001-lyrics-line-repair",
  "status": "queued",
  "updatedAt": "2026-06-27T00:00:00Z",
  "diagnostics": []
}
```

Valid statuses include `queued`, `waiting`, `resultFound`, `resultInvalid`,
`targetMismatch`, and `importedPendingSuggestion`.

## Result JSON

The external AI workflow writes the result file. It must be JSON matching the
ArtForge AI result contract documented in
`docs/prompting/ai-result-contract.md`. The result must either include the
request id or target metadata compatible with the request target. ArtForge
validates the JSON and imports valid candidates only as pending suggestions.

Manual queue mode never applies project edits automatically.
