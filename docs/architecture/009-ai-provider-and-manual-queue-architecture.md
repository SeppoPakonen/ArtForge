# AI Provider and Manual Queue Architecture

ArtForge treats AI execution as an adapter concern. Prompt package building,
result validation, pending suggestions, history, and editing remain local domain
services. Provider adapters only transport a prompt request and return a
validated AI result contract.

## Execution Modes

- `manualQueue`: first-class no-network mode. ArtForge writes request and prompt
  files into a user-owned folder and waits for an external chat or agent
  workflow to write a result JSON file.
- `openai`: future API adapter. It is optional and disabled unless explicitly
  configured.
- `anthropic`: future API adapter. It is optional and disabled unless explicitly
  configured.
- `alibabaCloud`: future API adapter. It is optional and disabled unless
  explicitly configured.

OpenAI, Anthropic, and Alibaba Cloud are provider adapters, not domain logic.
They must not change prompt package structure, pending suggestion rules, or file
editing rules.

## Provider Boundary

The UI and application services create an AI execution request containing:

- provider kind
- prompt package path and prompt summary
- target work path, domain, item type, item id, and item field
- requested operation
- expected AI result schema path
- expected result file path when the provider is file-backed

The provider returns a provider result containing:

- execution status
- provider kind
- request id
- request file path if one exists
- prompt text path if one exists
- result JSON path if one exists
- diagnostics
- validated pending suggestions when a result is available

All providers must return data that validates against the same AI result JSON
contract before ArtForge imports it as a pending suggestion. No provider may
apply edits directly.

## Manual Queue Mode

Manual queue mode requires no network access and no API credentials. It writes a
mechanically named set of files under a configurable user-owned queue root. The
default convention is:

```text
%USERPROFILE%\Documents\ArtForge\ai-queue
```

Each queued request receives a monotonically increasing six-digit sequence and a
safe slug derived from the requested operation and domain.

Example names:

```text
000001-lyrics-line-repair.request.json
000001-lyrics-line-repair.prompt.txt
000001-lyrics-line-repair.status.json
000001-lyrics-line-repair.result.json
```

Example layout:

```text
ai-queue/
  000001-lyrics-line-repair.request.json
  000001-lyrics-line-repair.prompt.txt
  000001-lyrics-line-repair.status.json
  000001-lyrics-line-repair.result.json
  pending-suggestions.jsonl
```

The prompt text file is intended for copy-paste into an external AI tool. The
external tool writes the result JSON file. ArtForge polls the expected result
path conceptually about once per second from the UI layer, but the core polling
service is a single explicit poll operation so it remains headless-friendly and
testable.

Manual status values:

- `queued`: request and prompt files exist, no result yet
- `waiting`: poll found no result file yet
- `resultFound`: result file exists
- `resultInvalid`: result file failed JSON contract validation
- `targetMismatch`: result target does not match the queued request target
- `importedPendingSuggestion`: valid result was imported as pending suggestion

## Security Rules

API secrets must never be stored in ArtForge project files, prompt packages,
queue files, examples, logs, history events, or source-controlled config. Future
API adapters must read credentials from environment variables, an OS credential
store, or a user-local ignored configuration file.

Manual queue files are intentionally human-readable and git-friendly, but they
can contain creative project content. The queue root is user-owned and should
not be placed inside a shared project repository by default.

## Non-Goals

This architecture does not implement network API calls. Provider configuration
and dispatcher stubs may exist, but API execution must remain disabled until a
later task adds explicit credential handling and network transport.
