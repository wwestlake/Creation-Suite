# Graph symbols: params, constants and variables

**Suite standard (owner decision, 2026-09-30): every node-based system in the suite supports graph symbols, the
same way.** Materials, Image Graph, Engine's behaviour graphs, Station's Foley graphs and any future node system.
The reference implementation is Djehuti Texture's Image Graph.

The model follows the research node language's schematic -
FrustLang `projects/10_node_compiler/NODE_SCHEMATIC_SCHEMA_V2.md` (`params`, `constants`, `variables`,
`accessibility`) - so a suite graph lifts into that system unchanged.

## What a symbol is

A named value that belongs to the graph itself (`node_system/symbols.h`, owned by `Graph`):

| Kind | Meaning |
|------|---------|
| **Param** | An input to the graph. Its value comes from outside - an Automation, the LLM, a parent graph - and `value` is the default used when nothing sets it. |
| **Constant** | A named fixed value used in several places. |
| **Variable** | Named state the graph can read and change while it runs (behaviour / execution graphs). A pure data-flow graph reads it like a constant. |

Fields: `id` (stable, no spaces; Get nodes refer to it, so renaming never breaks a graph), `name` (display),
`kind`, `type` (the node system's `DataType`), `value`, `accessibility` (schema v2: private, graph, module,
project, public, **agent**, readonly, hidden), `persistent` (variables), `description`.
`MakeSymbolId` makes a unique id from a name ("Tile Scale" -> `tile_scale`, then `tile_scale_2`).

## In the graph file

`frgraph` carries them, after the `target` line, only when a graph has any (older graphs are byte-for-byte
unchanged):

```
symbol param tile_scale float agent 0 default float 4
symbolname tile_scale Tile Scale
symboldescription tile_scale How many times the pattern repeats.
```

`symbol <kind> <id> <dataType> <accessibility> <persistent 0|1> [default <valueKind> <value>]`, then its name and
(optional) description lines. So every app that saves a `Graph` saves its symbols with no extra work.

## Reading a symbol: Get nodes

`node_system/symbol_nodes.h` registers one Get node per type - `core.symbol.get.float`, `.int`, `.bool`, `.color`,
`.vec3`, `.string` - each with a `symbol` input (the id) and a typed `value` output that wires into any matching
input. `SymbolForGetNode` finds the symbol a Get node reads; `AddSymbolGetNode` creates one already bound.

## The shared UI

- `creation::node_editor_ui::SymbolsPanel` - **the Variables panel**. Lists the graph's symbols (P / C / V badge,
  name, type, value); "+" adds a param, constant or variable of any type the app allows (`setAllowedTypes`);
  selecting one edits name, kind, type, value, access, persistent and description; a symbol's type cannot change
  while Get nodes use it (their wires would break). Dragging a row onto the graph adds a bound Get node.
- `NodeGraphComponent` accepts that drag (`kSymbolDragPrefix`, "symbol:<id>") and titles every Get node with its
  symbol's name - or "Missing symbol" in red if the symbol was removed.

## What an app must do

1. Register the Get nodes: `ce::node_system::RegisterSymbolGetNodes(registry)`.
2. Host a `SymbolsPanel` on the graph as a dockable panel titled **Variables**; on `onSymbolsChanged`, mark the
   document edited and re-evaluate / recompile. Whenever the graph's nodes change, call `graphChanged()` so the
   panel's use counts and type lock stay current.
3. Give Get nodes their value in its evaluator or code generator: `SymbolForGetNode(graph, node)->value`, except a
   **param**, whose value may be overridden from outside (Automations, the LLM) - the override wins.
4. Let data flow along wires into any setting input (a Get node, or a value node, wired into a node's setting
   replaces the typed-in value).

Nothing else is needed for save/load: symbols live in the `Graph`.

## Enums: named choices instead of numbers

A setting that picks one of several options (a blend mode, a direction) is an **enum**, never a bare integer
(`node_system/enums.h`). This follows schema v2's `enums` (a name and its variants).

- `EnumDef { name, displayName, variants, description }`. Variant i is the value i.
- A node type registers its enums with `NodeTypeRegistry::RegisterEnum`. An enum setting is an `Int` pin whose
  `PinTypeDesc::enumType` names the enum. Wires, files and generated code still carry the number.
- **Wiring:** two different enums never connect. A plain integer connects to an enum both ways.
- **frgraph:** `pin ... data int enum BlendMode default int 1`, and `symbolenum <id> <EnumName>` for a Choice symbol.
- **Properties** shows an enum setting as a dropdown of its variants. `PinEnum(registry, node, pin)` finds the enum
  (the pin's own tag, else its node type's signature).
- **Choice symbols:** the Variables panel lists every registered enum as a **Choice** type (`SymbolsPanel::setEnums`).
  A Choice's value is a dropdown, and its Get node carries the enum, so it wires only into settings of that enum or
  plain integers.

What an app must do: register its enums next to its node types, tag enum settings with `enumType`, call
`SymbolsPanel::setEnums(registry)`, and show `PinEnum` settings as dropdowns in its properties UI.
