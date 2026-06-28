# Command Strip Grouping

The current shell keeps a native Win32/Common Controls command strip instead of
introducing Ribbon UI.

Commands are grouped consistently in every scope app:

- File: Open, Save, Refresh
- AI: Build Prompt, Queue Manual
- Suggestion: Accept, Reject

The command bar wrapper owns the visual grouping. Shell command availability
still comes from presentation state: disabled Save, AI, and Suggestion actions
remain disabled until the selected scope and work item state make them valid.

This is intentionally a modest polish step. Icons and a larger command surface
can be reconsidered after layout, redraw, and editing workflows are stable.

