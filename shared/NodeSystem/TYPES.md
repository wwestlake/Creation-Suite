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

### Values that carry data (sum types)

Enums are full sum types, the way FRust's are (FRUST_LANG_SPEC.md 5.2). A value can carry values of its own:
none, one or several, each with a name and any type (a number, an image, a struct, another enum, or the enum
itself, so recursive types like a List or a Tree work).

- **Model:** `EnumVariant::fields`, each an `EnumField { name, type, description }`. `EnumCarriesValues(def)`
  says whether any value carries something.
- **FRust:** `enum Fill { Nothing, Solid(Array<f64, 3>, f64), Picture(i64) }`. Payloads are positional in
  FRust; the names are for people and pins.
- **frgraph** (after each value's own lines):

  ```
  enumvariant Fill Solid
  enumfield Fill 1 color
  enumfieldname Fill 1 0 colour
  enumfield Fill 1 float
  enumfieldname Fill 1 1 amount
  ```
- **On a wire, the value is still its number,** so it drives a Switch, compares and reads as an integer like any
  enum. What it carries travels alongside the number, so nothing is lost passing through a Switch, a struct
  member or a param.
- **A Choice param** of such an enum keeps what its chosen value carries in `memberValues` (`symbolmember`
  lines). Choosing another value starts those afresh.
- **Nodes:**
  - **Make Variant:** choose the enum and the value in Properties. What that value carries comes in as inputs;
    the enum value goes out.
  - **Match:** the enum value in; what every value carries out, as `<Value>_<field>` pins. Only the chosen
    value's pins carry anything, like a Route's unchosen outputs; asking another value's pin says which value
    was chosen. To branch on the value, wire it into a Switch: its cases are the values.
  - `SyncEnumNodePins` keeps their pins in line with the enum, matching by name like the struct nodes (renaming
    a field keeps its wires). Renaming a value updates Make Variant nodes set to it.
- **The Enum Editor:** "+" on a value adds something it carries; each one has a name and a type.
- A field can't be called `type`, `variant` or `value`, because Make Variant and Match use those names.

## Structs

- `StructDef`: **`name`** (stable), **`displayName`**, **`description`**, and **`members`**, each with a **name**, a
  **type** (any value type, an image, a drawing, a brush, an enum or another struct), a **default**, a
  **description** and an optional **range**.
- A struct can't hold itself. A member can't be called `type`, `value`, `members` or `member`, because the struct
  nodes use those names for their own pins.
- **frgraph** (a graph's own structs, after its enums):

  ```
  struct SurfaceSettings
  structname SurfaceSettings Surface Settings
  structmember SurfaceSettings float default float 0.5
  structmembername SurfaceSettings 0 amount
  structmember SurfaceSettings int enum BlendMode default int 0
  structmembername SurfaceSettings 1 mode
  ```
- **A struct param** stores its members' values in member order:

  ```
  symbolstruct look SurfaceSettings
  symbolmember look 0 default float 0.25
  ```
- **FRust:** `FrustStructDeclaration(def)` gives `struct SurfaceSettings { amount: f64, mode: BlendMode }`. Images,
  drawings and brushes become `i64` handles.

### Struct nodes (modelled on UE4)

Struct nodes only use a struct; they never make the type. Each one has a fixed **type** pin naming the struct,
chosen in Properties.

- **Make Struct:** members in, the struct out.
- **Break Struct:** the struct in, every member out.
- **Set Members:** the struct in, plus only the members ticked in Properties; the struct out with those changed and
  the rest passed through.
- **Get Member:** the struct in, one member (chosen in Properties) out.

A struct param's Get node gives the whole struct. It can go along one wire, or be taken apart with the nodes above.
`SyncStructNodePins` keeps the member pins in line with the struct, matching members by name:
- reordering a struct keeps every wire;
- a renamed member keeps its pin and wire (the Types panel also updates Set Members' ticks and Get Member's choice);
- a retyped member drops only the wires that no longer fit.

When a struct node's input isn't wired, it starts from the struct's defaults.

## Where they show

- **Variables panel:** every enum in scope is a **Choice** type. A Choice param's value is a dropdown.
- **Enum settings:** a dropdown in Properties.
- **Variables panel:** every struct in scope is a **Struct** type. A struct param shows one editor per member.
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
- **Struct Editor:**
  - display name, FRust name, scope and description, as for enums;
  - members: name, type, default and description, moved up or down, removed, added;
  - how often this graph uses it;
  - a live FRust preview;
  - Remove: struct params of it become plain numbers, and struct nodes set to it lose their pins.

### What an app does

1. Host a `TypesPanel` on its graph. On `onGraphTypesChanged`, mark the document edited and refresh what shows types
   (Variables, Properties).
2. Load and save the project's types (`project.frtypes`). Hand them to the panel (`setProjectTypes`) and to the
   registry (`ReplaceEnums` / `ReplaceStructs(project, ...)`), and save on `onProjectTypesChanged`.
3. Look types up through the graph (`PinEnum(graph, ...)`, `FindEnumFor`, `FindStructFor`).
4. Register the struct and enum nodes (`RegisterStructNodes`, `RegisterEnumNodes`) for its graph types, and call
   `SyncStructNodePins` / `SyncEnumNodePins` after each edit (as for `SyncFlowNodeCases`). Its evaluator carries struct values; Texture's image graph keeps a
   `StructValue` per wire.

## Better than UE4's user-defined enums and structs

- **They compile to real FRust types,** so code and graphs share them.
- **Names are stable,** so renaming never breaks a use.
- **Reordering or removing values remaps what's stored,** instead of silently changing meanings.
- **The editor shows how often the graph uses a type.**
- **Colours per value** carry through to Switch cases.
- **They're plain text in the graph and project,** so they're diff-able, and readable and writable by the LLM.
