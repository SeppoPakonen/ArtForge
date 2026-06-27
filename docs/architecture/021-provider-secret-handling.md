# Provider Secret Handling

ArtForge API providers are optional adapters. Credentials are user-local runtime
inputs and must never become project data.

## Allowed Secret Sources

The first implementation may read provider credentials from environment
variables:

- `ARTFORGE_OPENAI_API_KEY`
- `ARTFORGE_ANTHROPIC_API_KEY`
- `ARTFORGE_ALIBABA_CLOUD_API_KEY`

Future implementations may add:

- Windows Credential Manager or another OS credential store.
- User-local ignored configuration under a profile directory.

Provider configuration stored in the repository may contain only non-secret
fields such as provider kind, display name, endpoint profile name, model name,
and enabled/disabled state.

## Forbidden Secret Locations

Real credential values must not be stored in:

- `.af*` project files or scope files
- examples
- docs with real values
- prompt packages
- history events
- manual queue request, status, prompt, or result files
- source code
- Visual Studio project files
- git-tracked configuration
- logs or diagnostics

Diagnostics may say that a provider is configured or not configured. They may
name the source, such as `ARTFORGE_OPENAI_API_KEY`, but must not echo the value.

## Placeholder Examples

Use placeholders only in documentation:

```powershell
$env:ARTFORGE_OPENAI_API_KEY = "<set in user environment only>"
$env:ARTFORGE_ANTHROPIC_API_KEY = "<set in user environment only>"
$env:ARTFORGE_ALIBABA_CLOUD_API_KEY = "<set in user environment only>"
```

Do not commit shell profiles, `.env` files, or local provider config files that
contain real values.

## Runtime Rules

- Manual queue mode must work without any API credential.
- API providers remain disabled unless explicitly configured.
- Missing credentials return `notConfigured`.
- Authorization headers are created only at the transport boundary.
- Authorization headers are not written to history, examples, logs, prompt
  packages, or diagnostics.
- Provider call history records safe metadata only: provider kind, request id,
  non-secret model name, normalized status, duration, and diagnostic code.

## Future User-Local Config

If ArtForge later supports local provider config files, they must live outside
source-controlled project content by default. If a developer chooses to keep
local files near a checkout, `.gitignore` protects common names such as:

- `.artforge-local/`
- `.artforge-secrets/`
- `artforge.local.json`
- `artforge.providers.local.json`
- `*.secrets.json`
