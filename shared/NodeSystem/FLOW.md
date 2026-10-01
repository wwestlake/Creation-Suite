# Decisions in data-flow graphs: Switch and Route

**Suite standard (owner, 2026-10-01).** A decision in a data-flow graph chooses **which data flows**, not which
code runs. Think of F#'s `let r = if c then f x else g x`, or a `match`: each branch is a function call, only the
chosen one is evaluated, and its result is the value. Branches that are not chosen do nothing.

`node_system/flow_nodes.h`.

## The nodes

- **Switch** (many in, one out) is the `match`. A **selector** picks one of its **case** inputs, and **only that
  input is computed**. The output is that case's value.
- **Route** (one in, many out) is the split. Its input goes out on the case output the selector picks. The other
  outputs **carry nothing**, so nothing downstream of them is computed. Where the branches meet again, a Switch on
  the same selector picks the live one.
- There is **one node type per kind of value**, so wires stay type-safe: `core.switch.image`, `core.route.number`
  and so on, shown as "Switch (Image)", "Route (Number)", category **Flow**. Register them with
  `RegisterFlowNodes(registry, types, diagramTypes)`; `StandardFlowType(DataType)` gives the usual names.

## The selector and the cases

- **The selector takes any value.** An integer picks case n, a number case floor(n), and a toggle Off or On.
  Anything out of range is clamped (`FlowCaseIndex`).
- **The cases are named after what drives the selector** (`FlowCaseNames`):
  - an **enum**, such as a Choice param from the Variables panel, gives its variants (spaces become `_`);
  - a **toggle** gives `Off` and `On`;
  - anything else gives `case_0`, `case_1` ..., one for each of the node's **cases** setting.
- **`SyncFlowNodeCases(graph, registry, node)`** keeps the case pins in line. Pins whose names still match keep their
  wires and values. Call it when a flow node is added, when what's wired into its selector changes, or when its
  cases count changes.

## What an app does

1. Register the flow nodes for the value types its graphs carry, in its graph types.
2. Sync flow nodes' cases whenever the graph changes.
3. Evaluate or compile them **lazily**:
   - **Switch:** compute the selector, then only the chosen case.
   - **Route:** a wanted output that is not the chosen one is "not chosen". Report that without computing anything
     upstream.
   - A compiler (GLSL) may emit a select over all cases, since a shader computes every branch anyway. The result is
     the same.
4. Show what's not chosen as not chosen (for example on the nodes fed by it), not as an error.

Conditions come from ordinary value nodes: Compare (a > b → toggle), Logic (and / or), Math. Djehuti Texture's Image
Graph is the reference implementation.
