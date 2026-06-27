# AI Provider Configuration Stubs

ArtForge supports provider-neutral AI execution requests. API-backed providers
are planned for OpenAI, Anthropic, and Alibaba Cloud, but network execution is
not implemented yet.

The source-controlled provider configuration model may contain only non-secret
fields:

- provider kind
- display name
- endpoint or profile placeholder
- model name placeholder
- enabled flag

Credential values must come later from environment variables, an OS credential
manager, or a user-local ignored configuration file. They must not be stored in
project files, examples, logs, prompt packages, queue files, or history events.

Default API provider stubs are disabled. A disabled provider reports
`notConfigured`; an enabled API provider still reports `notImplemented` until a
future network transport task exists.
