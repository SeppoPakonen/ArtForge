# Presentation Adapter Architecture

ArtForge views must render presentation models. They must not interpret domain
models, file models, prompt package internals, or history records directly.

The mandatory layering is:

```text
Model
Application Services / Controllers
Presentation Model
Presentation Adapter
View
```

## Layer Responsibilities

### Model

Models hold authored project data and domain concepts: scope files, work domain
content, prompt package structures, history events, and dependency pressure
data. Models do not know about windows, controls, console output, or test
fixtures.

### Application Services / Controllers

Application services execute use cases. They load files, validate requests,
coordinate domain modules, call prompt builders, append history records, and
return service results. They should expose deterministic request and result
types that can be used by Win32 shells, command-line tools, tests, and future
automation.

Application services do not depend on `HWND`, Win32 controls, console streams,
or view classes.

### Presentation Model

Presentation models are UI-neutral state shaped for display and interaction.
Examples include status text, navigation tree nodes, table columns and rows,
property lists, command availability, selected item state, prompt preview
summaries, and diagnostics.

Presentation models should be plain C++ data. They may use standard library
containers and ArtForge value types, but they must not include Win32 headers.

### Presentation Adapter

Presentation adapters convert application service results and domain-facing
models into presentation models. They are the boundary where ArtForge decides
which fields become navigation entries, table cells, property rows, status
messages, prompt preview rows, or diagnostics.

Every view path must pass through a presentation adapter:

- Native Win32 views render presentation models.
- Console views print presentation models.
- JSON/debug views serialize presentation models.
- Tests inspect presentation models.

### View

Views render already-shaped presentation models and relay user gestures back as
commands or requests. A view may know how to populate a Win32 list view, property
panel, tab control, console table, or JSON debug dump. It must not parse or
reinterpret ArtForge domain objects to decide what a lyric line, visual layer,
script block, pressure package, or prompt layer means.

## Allowed Dependencies

Allowed dependency direction:

```text
View -> Presentation Adapter -> Presentation Model -> Application Service Result
Application Service -> Model modules
Presentation Adapter -> Model modules only when adapting a model already
                         returned by a service or loader
```

Not allowed:

- Presentation models depending on Win32, console output, or test frameworks.
- Application services depending on `HWND`, Win32 controls, or view classes.
- Views inspecting file/domain/prompt/history structures to build their own
  navigation, tables, selected-item properties, or command diagnostics.
- Domain libraries depending on UI modules.

## Example Flow

Selected lyric line flow:

1. The Win32 list view reports that row `0` was selected.
2. The view sends a selection request containing the work path, domain, item
   type, and row/index to the application boundary.
3. The application service validates the request against the loaded work data.
4. The presentation adapter converts the selected lyric line into a
   `SelectionModel` and a `PropertyListModel`.
5. The property model contains rows such as `Domain`, `Section`, `Time`, `Text`,
   and `Line`.
6. The Win32 property panel renders those rows without knowing lyric schema
   details.

This keeps the same selection and property logic available to the Win32 UI,
headless command-line output, JSON/debug output, and tests.
