# API Provider Implementation Investigation

This investigation covers future network provider implementations for OpenAI,
Anthropic, and Alibaba Cloud Model Studio. It does not implement network calls.
The current no-network dispatcher and manual queue remain the default path.

Sources checked on 2026-06-27:

- OpenAI Responses API reference:
  <https://platform.openai.com/docs/api-reference/responses/create>
- OpenAI Structured Outputs guide:
  <https://platform.openai.com/docs/guides/structured-outputs>
- Anthropic Messages API and structured output documentation:
  <https://docs.anthropic.com/>
- Alibaba Cloud Model Studio DashScope API reference:
  <https://www.alibabacloud.com/help/en/model-studio/qwen-api-via-dashscope>
- Alibaba Cloud OpenAI-compatible interface:
  <https://www.alibabacloud.com/help/en/model-studio/compatibility-of-openai-with-dashscope>
- Microsoft WinHTTP overview and SSL documentation:
  <https://learn.microsoft.com/en-us/windows/win32/winhttp/about-winhttp>
  and <https://learn.microsoft.com/en-us/windows/win32/winhttp/ssl-in-winhttp>

## Current ArtForge Boundary

Provider adapters should sit behind the existing provider-neutral model:

- `AiExecutionRequest`
- `AiExecutionResult`
- `AiProviderConfiguration`
- validated AI result JSON
- pending suggestion import
- provider execution status history

Providers transport a prompt request and return JSON. They must not apply
suggestions, modify work files, or bypass pending suggestion review.

## HTTP And TLS Options

Recommended first implementation: WinHTTP.

Reasons:

- it is available in Windows and works with Visual Studio C++ without adding a
  third-party dependency;
- it supports HTTPS/TLS through the Windows platform trust store;
- it is suitable for native Win32 desktop code and service-style HTTP calls;
- it avoids bringing libcurl/OpenSSL packaging into the early modular phase.

Future alternatives:

- C++ REST SDK or another maintained HTTP wrapper if ArtForge later needs a
  higher-level abstraction;
- libcurl only if cross-platform packaging becomes a real requirement;
- Windows Web Services or WinINet are not preferred for this provider boundary.

The provider adapter should expose a narrow internal interface so the HTTP
backend can be replaced later.

## Credential Handling

No provider implementation may store secrets in project files, examples, prompt
packages, history events, logs, source code, or Visual Studio project files.

Future credential sources, in preferred order:

1. Windows Credential Manager or another OS credential store.
2. Environment variables read at runtime.
3. User-local ignored configuration under a profile directory.

History and diagnostics may record credential source names such as
`user-local-openai-profile`, but never the secret value.

## Provider Notes

### OpenAI

Use the Responses API as the primary target. The existing ArtForge prompt
package maps naturally to a single request with instructions, input context,
and a schema-constrained JSON output requirement. Structured Outputs should be
used when practical so the provider adapter receives JSON matching the
ArtForge AI result contract before validation.

Mapping:

- provider kind: `openai`
- request id: generated ArtForge execution id
- model: from user-local provider config
- input: prompt package markdown and technical JSON context
- output: raw JSON text, then `ValidateAiResultJsonText`

### Anthropic

Use the native Messages API rather than an OpenAI compatibility layer for the
first full implementation. Anthropic documents structured-output approaches for
schema-constrained JSON, which is a better match for ArtForge than relying only
on prompt text.

Mapping:

- provider kind: `anthropic`
- request id: generated ArtForge execution id
- model: from user-local provider config
- messages/system content: prompt package layers
- output: JSON text or structured-output payload, then ArtForge validation

### Alibaba Cloud Model Studio

Alibaba Cloud Model Studio supports DashScope APIs and OpenAI-compatible
interfaces for Qwen models. The first ArtForge implementation should prefer the
OpenAI-compatible interface only if its behavior matches ArtForge's required
JSON-output validation and role mapping. Otherwise use the native DashScope API.

Mapping:

- provider kind: `alibabaCloud`
- request id: generated ArtForge execution id
- model/base URL: from user-local provider config
- output: JSON text, then ArtForge validation

## Request And Response Mapping

Provider request:

- provider kind
- request id
- prompt package path/id
- target work/domain/item metadata
- requested operation
- schema path or embedded schema text

Provider response:

- transport status
- provider status category
- raw response path if persisted
- diagnostics without secrets
- validated pending suggestions

The response must pass the existing AI result JSON validation before import.
Invalid JSON or schema mismatch produces diagnostics and no pending suggestion.

## Error Categories

Required normalized categories:

- `notConfigured`: provider disabled or missing user-local credentials/config.
- `authFailed`: authentication rejected by provider.
- `rateLimited`: quota or rate-limit response.
- `networkFailure`: DNS, TLS, timeout, or connection failure.
- `invalidResponse`: response body is malformed or missing expected content.
- `schemaValidationFailed`: provider returned JSON that does not satisfy the
  ArtForge AI result contract.
- `providerError`: provider-specific error not covered above.

These categories should map to `AiExecutionResult` status/diagnostics and
provider execution history events.

## Staged Implementation Order

1. Add an HTTP transport abstraction with no provider credentials.
2. Add OpenAI provider behind a disabled-by-default config.
3. Add Anthropic provider using native Messages API.
4. Add Alibaba Cloud provider, starting with OpenAI-compatible mode only if it
   passes JSON contract tests.
5. Add retry/backoff policy and cancellation.
6. Add UI for provider configuration status without exposing secrets.

## Out Of Scope

- network calls in the current task
- storing API credentials
- automatic suggestion application
- automatic merge or conflict resolution
- full undo/redo restoration
