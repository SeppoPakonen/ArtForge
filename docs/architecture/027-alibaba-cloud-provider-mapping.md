# Alibaba Cloud Provider Mapping

This investigation documents how future Alibaba Cloud Model Studio / DashScope
support should map to ArtForge's provider-neutral prompt and AI result
contracts. It does not implement network calls.

Sources checked on 2026-06-27:

- Model Studio OpenAI-compatible Chat:
  <https://www.alibabacloud.com/help/en/model-studio/compatibility-of-openai-with-dashscope>
- Model Studio OpenAI-compatible Responses:
  <https://www.alibabacloud.com/help/en/model-studio/compatibility-with-openai-responses-api>
- Model Studio text generation API reference:
  <https://www.alibabacloud.com/help/en/model-studio/qwen-api-reference/>
- Model Studio DashScope API reference:
  <https://www.alibabacloud.com/help/en/model-studio/qwen-api-via-dashscope>
- Model calls in sub-workspaces:
  <https://www.alibabacloud.com/help/en/model-studio/model-calling-in-sub-workspace>

## Recommended Staged Approach

Start with OpenAI-compatible mode if local fixtures prove it preserves the
ArtForge JSON result contract:

1. Reuse the OpenAI-compatible request mapping shape where possible.
2. Configure provider kind `alibabaCloud`, base URL, region/workspace profile,
   and Qwen model name separately from OpenAI.
3. Reuse staged response validation: transport, provider response parse,
   candidate extraction, ArtForge contract validation, target compatibility.
4. Add DashScope-native mapping only if OpenAI-compatible mode cannot satisfy a
   required Qwen feature or strict JSON result behavior.

This keeps the first provider adapter small and makes OpenAI-compatible
behavior testable with the same fixture style already used for OpenAI.

## Configuration Fields

Source-controlled provider configuration may contain only non-secret values:

- provider kind: `alibabaCloud`
- display name: `Alibaba Cloud`
- endpoint profile: a named local profile or explicit base URL
- model name: for example `qwen-plus` or another selected Qwen model
- region/workspace hint: for example default Beijing or a sub-workspace profile
- enabled flag

Credential values must come from user-local runtime configuration. Do not store
them in ArtForge project files, examples, prompt packages, history events, or
logs.

The endpoint profile needs to distinguish at least:

- default OpenAI-compatible base URL
- OpenAI-compatible Responses path
- sub-workspace base URL such as a Singapore workspace endpoint
- later DashScope-native endpoint, if needed

## Request Mapping

Preferred first request path: OpenAI-compatible Responses, if fixture tests show
it accepts the ArtForge structured result prompt reliably.

High-level request:

```json
{
  "model": "qwen-placeholder",
  "stream": false,
  "input": [
    {
      "role": "developer",
      "content": "Return exactly one ArtForge AI result JSON object."
    },
    {
      "role": "user",
      "content": "Layered ArtForge prompt package content."
    }
  ]
}
```

Fallback request path: OpenAI-compatible Chat Completions.

```json
{
  "model": "qwen-placeholder",
  "stream": false,
  "messages": [
    {
      "role": "system",
      "content": "ArtForge provider instructions and output contract."
    },
    {
      "role": "user",
      "content": "Layered ArtForge prompt package content."
    }
  ]
}
```

DashScope-native APIs should be a later path only when OpenAI-compatible mode is
insufficient. DashScope-specific feature fields must not leak into the
provider-neutral `AiExecutionRequest`; keep them inside an Alibaba adapter
mapping layer.

## Response Extraction

Responses-compatible mode should follow the same extraction strategy as OpenAI:

1. Detect provider error object before candidate parsing.
2. Extract `output_text` or equivalent output content.
3. Extract exactly one complete JSON object candidate.
4. Reject markdown, empty output, malformed JSON, or multiple competing JSON
   candidates.
5. Validate with `ValidateAiResultJsonText`.
6. Verify target metadata against the execution request.

Chat-compatible mode should extract assistant message content from
`choices[0].message.content` for non-streaming responses. Streaming is out of
scope for the first implementation.

## Error Handling

Normalize Alibaba Cloud errors to existing ArtForge categories:

- missing runtime configuration: `NotConfigured`
- rejected credentials or permission issue: `Failed` with `authFailed`
- quota/rate limit: `Failed` with `rateLimited`
- regional endpoint, workspace, or model permission issue: `Failed` with
  `providerError`
- network/TLS/timeout: `Failed` with `networkFailure`
- malformed provider body: `ResultInvalid`
- ArtForge schema mismatch: `ResultInvalid`
- target mismatch: `TargetMismatch`

History events should record only safe metadata: provider kind, request id,
prompt package path, model name, endpoint profile name, status, duration, and
diagnostic code. They must not store authorization headers, credential values,
full prompt text, or raw provider response bodies.

## Regional And Workspace Concerns

Alibaba Cloud configuration is more region-sensitive than the OpenAI path:

- Default and sub-workspace endpoints can differ.
- Workspace-specific credentials and permissions can restrict available models.
- International and China-region endpoints can require different base URLs.
- Model names and feature support can differ across model families.

ArtForge should treat `endpointProfile` as a named user-local profile first,
not as a project-owned URL. A future UI can display the profile name and
configured/not configured status without revealing credential values.

## Risks

- OpenAI-compatible Responses may not exactly match OpenAI behavior for every
  structured output option.
- Built-in tool features are not needed for ArtForge's first provider path and
  should remain disabled.
- Region/workspace mistakes can look like authentication failures unless
  diagnostic codes are normalized carefully.
- Multimodal Qwen models may expose DashScope-specific fields not represented
  by the provider-neutral text-first contract.
- Streaming and batch interfaces have different response shapes and should not
  be mixed into the first non-streaming implementation.

## Next Implementation Step

Before any Alibaba Cloud network implementation, add local fixtures for:

- OpenAI-compatible Responses success
- OpenAI-compatible Chat success
- provider error object
- malformed content
- ArtForge contract mismatch
- target mismatch
- region/workspace/model permission failure

Only after fixture validation should an opt-in provider adapter be added.
