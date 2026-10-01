# Graph types, graph interfaces and graphs as nodes

**Suite standard (owner decision, 2026-10-01):** the suite has one node system. A graph has a **type** - what kind of
thing it makes - and the type picks the nodes that belong in it. A graph has an **interface** (inputs and outputs),
so a graph can be used as a **node** in another graph, including a graph of another type. That hierarchy is how
complexity is built in an organised way. Whatever a graph makes, it shows in its own form: images as pictures,
materials as lit previews, drawings as lines.

It follows the research node language's schematic (FrustLang `projects/10_node_compiler/NODE_SCHEMATIC_SCHEMA_V2.md`):
`diagramType`, `subgraphs` with declared inputs / outputs, `call_function` nodes and "Convert to Function".

Built in phases. Djehuti Texture is the first user, as it was for Variables (SYMBOLS.md):

1. **Graph types** - done (this document, below).
2. Graph interface and the Graph node (a graph used inside a graph of the same type).
3. One graph editor in Texture for every type (Materials and Image Graph merge).
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
