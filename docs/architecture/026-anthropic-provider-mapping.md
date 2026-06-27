# Anthropic Provider Mapping

This investigation documents how a future Anthropic adapter should map to
ArtForge's provider-neutral prompt and AI result contracts. It does not
implement network calls.

Sources checked on 2026-06-27:

- Anthropic Messages API reference:
  <https://docs.anthropic.com/en/api/messages>
- Anthropic OpenAI SDK compatibility notes:
  <https://docs.anthropic.com/en/api/openai-sdk>
- Anthropic API overview and structured output guidance:
  <https://docs.anthropic.com/claude/reference/getting-started-with-the-api>

## Provider Boundary

Use `AiProviderKind::Anthropic` behind the same provider-neutral objects used by
manual queue and OpenAI:

- `AiExecutionRequest`
- `AiExecutionResult`
- `AiProviderConfiguration`
- staged result diagnostics
- provider call history without secrets
- pending suggestion import only after ArtForge JSON validation

The adapter must not apply suggestions or write work files. It may produce
validated pending suggestions for later review.

## Request Shape

Prefer the native Anthropic Messages API over the OpenAI compatibility endpoint
for the first implementation. Anthropic's compatibility documentation says the
compatibility layer is mainly for evaluation, and documents differences such as
ignored `response_format` and hoisted system/developer messages.

High-level native request:

```json
{
  "model": "claude-placeholder",
  "max_tokens": 4096,
  "system": "ArtForge provider instructions and stable output rules.",
  "messages": [
    {
      "role": "user",
      "content": [
        {
          "type": "text",
          "text": "Layered ArtForge prompt package content."
        }
      ]
    }
  ]
}
```

The exact schema mechanism must be verified when implementation begins. If
Anthropic structured outputs are available for the selected model, use them.
Otherwise use a strict text instruction plus local `ValidateAiResultJsonText`
and reject anything that is not a complete ArtForge AI result JSON object.

## Prompt Layer Mapping

Map ArtForge prompt package layers this way:

- General creative rules: `system`, first section.
- Domain rules: `system`, after general rules.
- Artist or subject rules: `system`, after domain rules.
- Project or series rules: `system`, after artist rules.
- Work item state JSON: user content block with a clear `current_state` label.
- User markings and repair request: user content block with task instruction.
- Output contract JSON: system output rule plus user-visible schema reference.

Anthropic supports a single initial system message in native Messages usage, so
ArtForge should concatenate rule layers into a deterministic, labeled system
string. Current work state and selected item data should remain in user content
so the prompt package retains a clear division between durable rules and task
data.

## Response Extraction

The adapter should extract text from Anthropic response content blocks, then:

1. Detect provider error or refusal categories before parsing candidate output.
2. Extract exactly one JSON object candidate from text or structured output.
3. Reject empty text, multiple competing candidates, malformed JSON, or markdown
   fenced output.
4. Run the existing ArtForge AI result contract validation.
5. Verify target metadata against `AiExecutionRequest.target`.
6. Return `ResultFound`, `ResultInvalid`, `TargetMismatch`, or `Failed`.

Do not persist raw provider response bodies by default. If future diagnostics
need response capture, store only user-approved test fixtures or redacted
developer logs outside project files.

## Error Handling

Normalize Anthropic failures to existing ArtForge categories:

- missing local configuration: `NotConfigured`
- transport, TLS, timeout, or DNS failure: `Failed` with `networkFailure`
- provider authentication failure: `Failed` with `authFailed`
- rate limiting: `Failed` with `rateLimited`
- malformed response: `ResultInvalid`
- ArtForge contract mismatch: `ResultInvalid`
- target mismatch: `TargetMismatch`

History events should use provider call lifecycle operations and safe metadata:
provider kind, request id, prompt package path, model name, duration, status,
and diagnostic code. They must not include credential values, authorization
headers, raw prompt text, or raw provider response text.

## Differences From OpenAI

- OpenAI mapping currently targets Responses API `input` plus `text.format`.
- Anthropic native Messages uses a single `system` value and a `messages` array.
- Anthropic OpenAI compatibility is not the preferred production path because
  documented compatibility differences affect schema and system-message
  handling.
- Anthropic output extraction must handle content blocks rather than
  `output_text`.
- Tool or structured-output support must be validated against the selected
  Claude model before relying on schema conformance.

## Implementation Risks

- Structured output support may vary by model and API version.
- The adapter may need a small response parser for text and structured content
  blocks.
- Prompt-layer ordering must be deterministic to avoid changing creative output
  unexpectedly.
- Compatibility endpoint behavior could silently ignore fields that ArtForge
  expects for strict JSON output.
- Error shapes and refusal indicators must be captured by fixtures before any
  network implementation is enabled.

## Next Implementation Step

Add Anthropic mapping only after local fixtures exist for success, malformed
JSON, missing content, provider error, and target mismatch. The implementation
must stay disabled unless user-local configuration explicitly enables it.
