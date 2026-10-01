# Types: made in an editor, used wherever they are in scope

**Suite standard (owner, 2026-10-01).** Making a type is not part of any graph, and it is not a node. A type is
defined in a **type editor**. From then on it is one of the types things can be, wherever it is in scope: a param's
type, a setting's type, what a Switch or Route selector carries, and (with structs) a struct member's type. The node
system compiles types down to FRust: an enum becomes a FRust `enum`.

The model follows research's node schematic (FrustLang `NODE_SCHEMATIC_SCHEMA_V2.md`: `enums`, `structs`,
accessibility).

## Scope

Where a type is defined is where it can be used (`TypeScope`). A name defined closer in wins.

| Scope | Defined | Visible to |
|---|---|---|
| **graph** | in the graph, saved with it (`Graph::Enums`) | that graph |
| **project** | in the project's types file (`project.frtypes`, saved by the app) | every graph in the project, in any app |
| **pod** | in a pod | anything that uses the pod (phase 3) |
| **builtin** | by an app or library (`NodeTypeRegistry::RegisterEnum`) | that app's graphs |

Look a name up with `FindEnumFor(graph, registry, name)`, or `PinEnum(graph, registry, node, pin)` for a pin. They
check the graph's own enums first, then the registry, which holds the project's, the pods' and the built-in ones. An
app puts the project's enums into its registry with `ReplaceEnums(TypeScope::project, enums)` whenever it loads or
changes them.

## Enums

- `EnumDef`:
  - **`name`:** an identifier, stable once made. Pins and Choices refer to it, so renaming the display name never
    breaks anything.
  - **`displayName`** and **`description`**.
  - **`variants`:** each with a **name**, a **description** and an optional **colour**.
- **Value i is variant i.** That's how they're stored and how FRust numbers them. Moving or removing a value
  remaps what the graph stores (the Types panel does it), so a setting keeps meaning the same value.
- **frgraph** (a graph's own enums, before its symbols):

  ```
  enum HsvChannel
  enumname HsvChannel HSV Channel
  enumvariant HsvChannel Hue
  enumvariantcolour HsvChannel 0 ffe0605a
  enumvariantdescription HsvChannel 0 Where on the colour wheel
  enumvariant HsvChannel Saturation
  enumvariant HsvChannel Value
  ```
- **A types file** (project or pod scope) holds the same lines after `frtypes 1` (`SerializeTypes` /
  `DeserializeTypes`).
- **FRust:** `FrustEnumDeclaration(def)` gives `enum HsvChannel { Hue, Saturation, Value }`, with names made into
  identifiers by `FrustIdentifier`. The graph code generator emits a graph's own enums at the top of its source
  (`FrustEnumDeclarations`).

## Where they show

- **Variables panel:** every enum in scope is a **Choice** type. A Choice param's value is a dropdown.
- **Enum settings:** a dropdown in Properties.
- **Switch and Route:** an enum driving the selector names the cases, and renaming a value renames the case in place
  without losing its wire (FLOW.md).

## The editors (shared NodeEditorUI)

`TypesPanel` lists the types in scope, grouped as This graph / Project / Built-in, with built-in ones read-only.
"+" makes a new one in the graph or the project. Selecting one opens its editor:
- **Enum Editor:**
  - display name, with its fixed FRust name and scope shown beside it;
  - description;
  - values: colour, name and description, moved up or down, removed, added;
  - how often this graph uses it;
  - a live FRust preview;
  - Remove: whatever used it in this graph becomes a plain integer.
- **Struct Editor:** phase 2.

### What an app does

1. Host a `TypesPanel` on its graph. On `onGraphTypesChanged`, mark the document edited and refresh what shows types
   (Variables, Properties).
2. Load and save the project's types (`project.frtypes`). Hand them to the panel (`setProjectTypes`) and to the
   registry (`ReplaceEnums(project, ...)`), and save on `onProjectTypesChanged`.
3. Look enums up through the graph (`PinEnum(graph, ...)`, `FindEnumFor`).

## Better than UE4's user-defined enums and structs

- **They compile to real FRust types,** so code and graphs share them.
- **Names are stable,** so renaming never breaks a use.
- **Reordering or removing values remaps what's stored,** instead of silently changing meanings.
- **The editor shows how often the graph uses a type.**
- **Colours per value** carry through to Switch cases.
- **They're plain text in the graph and project,** so they're diff-able, and readable and writable by the LLM.
