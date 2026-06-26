# Pressure Package Types

ArtForge pressure packages describe creative requirements using a small
dependency vocabulary. They are not system packages and there is no dependency
solver in this phase.

## Minimal Model

- `PressurePackageId`: stable package identifier, such as
  `live-solo-performance`.
- `PackageKind`: creative requirement, work, rule pack, fragment, or project
  function.
- `PackageVersion`: human-readable package revision.
- `PressureFlag`: user choice that changes package behavior or requirements.
- `PressureSlot`: named role that compatible requirements or works can fill.
- `PressureDependency`: required, recommended, or conflicting package relation.
- `PressureBlocker`: package that prevents another package from being valid.
- `PressureDiagnostic`: severity, category, package id, and message suitable for
  later UI display or history recording.

## Examples

### Live Solo Performance Requirement

Package id: `live-solo-performance`

Flags:

- `solo_performer`: enabled
- `portable_arrangement`: enabled
- `wide_vocal_range`: disabled

Dependencies:

- Requires `singable-range` because the melody must stay within the artist's
  reliable live range.
- Requires `portable-instrumentation` because the arrangement must work without
  unavailable collaborators.

Blocker:

- `studio-only-layering` blocks the package when dense layered parts make a solo
  live version impractical.

### Low-Cost Music Video Requirement

Package id: `low-cost-music-video`

Flags:

- `few_locations`: enabled
- `daylight_shooting`: enabled
- `licensed_props`: disabled

Dependencies:

- Requires `few-location-plan` so the concept fits a small number of locations.
- Requires `daylight-availability` because the shoot plan assumes available
  daylight.

Blocker:

- `licensed-prop-dependency` blocks the package when props require licensing,
  rental cost, or approval overhead.

### Album Or Series Arc Pressure

Package id: `album-arc-pressure`

Flags:

- `opening_contrast`: enabled
- `midpoint_escalation`: enabled
- `closing_resolution`: enabled

Dependencies:

- Requires `opening-contrast` for a first work that establishes contrast.
- Requires `midpoint-escalation` for middle works that raise pressure.
- Requires `closing-resolution` for final work resolution.

Blocker:

- `flat-track-order` blocks the package when ordering removes the intended arc.

## Diagnostic Format

`FormatDiagnosticMessage` produces a compact string:

```text
[error] missing dependency for live-solo-performance: singable-range is missing
```

The formatted string is only for logs and early diagnostics. Later UI can render
the structured diagnostic fields directly.

`EvaluatePressureDiagnostics` performs a small direct pass over selected pressure
packages. It can report missing required dependencies, present blockers, enabled
flag conflicts, required slots that are not provided by any selected package, and
simple two-package circular `requires` cycles. It is not a solver and does not
choose automatic resolutions.

Sample diagnostic output is stored in
[sample-diagnostics.txt](sample-diagnostics.txt).

`BuildWorldUpdateSummary` groups those diagnostics into a compact world update
report with package counts, severity counts, packages needing attention, missing
dependencies, blockers, flag conflicts, missing slots, and circular dependency
placeholders. It is still a formatter over diagnostics, not a dependency
solver. Sample output is stored in
[sample-world-update-summary.txt](sample-world-update-summary.txt).
