# ArtForge Work Domain File Schemas

## Purpose

ArtForge work files keep a shared scope envelope and add a small
domain-specific payload for the current work type. The shared envelope remains
loadable by the existing scope loader:

- `format`
- `schemaVersion`
- `id`
- `scope`
- `workKind`
- `displayName`
- `series`
- `sources`
- `history`
- `metadata`

Domain examples add:

- `workDomain`: canonical domain discriminator
- `domain`: human-readable domain payload object

The current loader treats these fields as forward-compatible JSON. Full typed
domain parsing belongs in a later task.

## Domain Values

| Value | Meaning |
| --- | --- |
| `lyrics` | Music, lyrics, song sections, timed lines, and performer constraints. |
| `visualArt` | Visual artwork layers, construction layers, reference placeholders, and meaning categories. |
| `scriptStoryboard` | Scenes, time ranges, dialogue/action blocks, speakers, and storyboard placeholders. |

## Lyrics / Music Payload

Required minimal shape:

- `song`: title, language, tempo, and key metadata when known
- `sections`: ordered section ids and labels
- `lyricLines`: stable line ids with section id, optional time range, text, and evaluation placeholders
- `technicalBaseText`: plain text for analysis and prompt context
- `performerProfile`: relative reference to singer or performer constraints

## Visual Art Payload

Required minimal shape:

- `viewerLayers`: presentation layers visible to the viewer
- `paintLayers`: construction or paint layers used by the artist
- `meaningCategories`: first-glance, mid-depth, and deep-meaning notes
- `references`: optional image or source placeholders using relative paths

## Script / Storyboard Payload

Required minimal shape:

- `scenes`: ordered scene ids, titles, and time ranges
- `blocks`: dialogue or action blocks with stable ids, scene ids, speaker/voice, text, and time ranges
- `storyboard`: shot or visualization placeholders

## Examples

Minimal examples live under `examples/work-domains/`:

- `lyrics.afwork.json`
- `visual-art.afwork.json`
- `script-storyboard.afwork.json`
- `unsupported.afwork.json`

These files are intentionally small and deterministic. They are not full editor
documents yet.
