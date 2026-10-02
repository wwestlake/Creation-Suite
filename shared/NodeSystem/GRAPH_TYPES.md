# Graph types, graph interfaces and graphs as nodes

**Suite standard (owner decision, 2026-10-01):** the suite has one node system. A graph has a **type** - what kind of
thing it makes - and the type picks the nodes that belong in it. A graph has an **interface** (inputs and outputs),
so a graph can be used as a **node** in another graph, including a graph of another type. That hierarchy is how
complexity is built in an organised way. Whatever a graph makes, it shows in its own form: images as pictures,
materials as lit previews, drawings as lines.

It follows the research node language's schematic (FrustLang `projects/10_node_compiler/NODE_SCHEMATIC_SCHEMA_V2.md`):
`diagramType`, `subgraphs` with declared inputs / outputs, `call_function` nodes and "Convert to Function".

Built in phases. Djehuti Texture is the first user, as it was for Variables (SYMBOLS.md):

1. **Graph types** - done.
2. **Graph interface and the Graph node** - done (a graph used inside a graph of the same type).
3. **One graph editor in Texture for every type** - done (Materials and Image Graph merged).
4. Cross-type graph nodes (an Image graph as a texture source in a Material graph).
5. Convert to Graph (collapse selected nodes into a new graph, boundary wires becoming its pins).

## 1. Graph types

- `Graph::DiagramType()` / `SetDiagramType()` - a short id: `image`, `material`, later `behavior`, `foley`... Empty
  means untyped (anything goes), which is how every graph started out.
- **frgraph:** a `diagram <type>` line after `target`, written only when the graph has a type (untyped graphs are
  byte-for-byte unchanged).
- **Which nodes belong:** `NodeTypeDescriptor::diagramTypes` lists the graph types a node type is for. An empty list
  means every type - shared nodes such as the symbol Get nodes and plain values. `AllowedInDiagram(descriptor, type)`
  answers the question; an untyped graph allows everything.
- **The types an app offers:** `NodeTypeRegistry::RegisterDiagramType({ id, displayName, description })`.
- **The node list:** `NodePalette::SetDiagramType(type)` lists only the nodes that belong. `NodeGraphComponent`
  refuses a dropped node that does not belong in its graph.
- **Changing a graph's type:** allowed when `NodesNotAllowedIn(graph, registry, newType)` is empty; otherwise those
  nodes are what stand in the way - tell the person which.

Note that the graph type is separate from `GraphTarget` / a compile target (schema v2 keeps `diagramType` and
`targets` apart): an image graph is evaluated, a material graph compiles to GLSL, and one type may have several
targets.

### What an app does

1. Give each of its node types the graph types it belongs in (`diagramTypes`); leave shared nodes empty.
2. Register its graph types (`RegisterDiagramType`).
3. Set the type on every graph it makes or opens, and on its palette (`SetDiagramType`).

## 2. Graph interfaces and the Graph node

`node_system/graph_nodes.h`.

- **A graph's interface** (`InterfaceOf(graph, registry)`):
  - **Inputs:** its params (SYMBOLS.md), each named by its id, typed by the param (Choices keep their enum), with the
    param's value as the default. Then its **graph-input nodes**.
  - **Outputs:** its **graph-output nodes**.
- **Graph-input and graph-output nodes** are ordinary node types whose descriptor sets `graphPort`
  (`GraphPort::input` / `GraphPort::output`).
  - Each names its port with a text pin called `name` (spaces become `_`).
  - An input's type is its first output pin's type. An output's type is its first input pin's type other than `name`.
  - On its own, a graph-input node should still work, with a value of its own, so the graph can be opened and
    previewed by itself.
- **The Graph node** `core.graph` (`RegisterGraphNode(registry, diagramTypes)`, category "Graphs") has one fixed input,
  `graph`: which graph it uses, as a path the app resolves (a project asset). After that it has one pin per port.
  - Its descriptor has `dynamicPins`, so validation checks only the fixed pin.
  - `SyncGraphNodePins(host, node, interface)` brings its pins up to date. It keeps the ones that still match a port,
    with their wires and typed-in values. It removes the others along with their wires, and adds new ones.
  - Call it when the node's graph is chosen, and when a graph that uses others is opened, since those graphs may have
    changed.
- **Evaluating or compiling a Graph node is the app's job.** Run the used graph with its params set from the node's
  inputs (a param's outside value, as in SYMBOLS.md) and its graph-input nodes fed from the node's wired inputs. Take
  each wanted output from the graph-output node of that name.
  - Refuse a graph that uses itself, directly or through others; a depth limit does it.
  - A change to the used graph must change the result. Make the saved text of the used graph part of the node's cache
    key.

### What an app does (on top of section 1)

1. Mark its input and output node types with `graphPort`; give inputs a value of their own.
2. Register the Graph node for its graph types.
3. In the Graph node's own Properties, offer the saved graphs it can use (never the graph itself), and sync the pins.
4. Run a used graph in its evaluator or compiler as above.

Djehuti Texture's Image Graph is the reference: Graph Input and Output nodes, the Graph node in Properties, and nested
evaluation sharing one compiled FRust runtime (`image_graph::Evaluator`).

## 3. One editor for every type (Djehuti Texture)

Texture has one Graph editor (Layout > Graph, with its Draw view) for every kind of graph it makes: `image` and
`material`. Its single registry holds the image library, the material nodes (`ce::material::RegisterMaterialNodes`
types them `material` and registers the type), the shared Get nodes and the Graph node. The graph's type picks:

- **The node list:** `NodePalette::SetDiagramType`.
- **The Variables types:** materials use numbers and colours, which compile to shader uniforms (params) or literals
  (constants). `CompileMaterialGraph` handles symbol Get nodes.
- **The engine and preview:** an image graph is evaluated on a background thread and shown on the 2D Preview. A
  material is compiled to a shader and shown lit on the 3D Preview.
- **How it saves:** an image graph as an `.imggraph.json` document. A material as a material asset (`.frgraph`),
  the form other apps read. File > Open lists both kinds.

The separate Materials work area is gone.
