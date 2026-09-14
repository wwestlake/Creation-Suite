# Suite Node Authoring Design

Status: requirements baseline for review

This document defines two graph-authoring features that apply to every Suite
NodeSystem graph: attached comments and nestable node containers.

## 1. Attached comments

### 1.1 Comment node

A Comment is a first-class graph annotation with a multiline body, optional
title, author, color, and tags. It has no Data or Exec behavior.

Each Comment exposes a dedicated Annotation connector. The connector may attach
to any node or, in a later extension, to a specific pin. Annotation connections
are visually distinct from execution and data connections.

### 1.2 Semantics

* A node may have any number of attached comments.
* A comment may document multiple nodes.
* Annotation edges never participate in execution, data dependency ordering,
  cycle detection, code generation, or runtime scheduling.
* Comments and annotation connections serialize with the graph.
* Removing a node removes its annotation links; removing a comment removes only
  its annotation links.
* Copy/paste and container moves preserve comment content and attachment IDs.

Comments are explanatory graph data, not executable nodes. They must remain
available to FRust source comments, graph documentation, search, and future AI
assistance without changing runtime behavior.

## 2. Node containers

### 2.1 Composite node

A Node Container, also called a Composite node, owns a child graph and presents
that graph as one node in its parent graph. The container is therefore both:

* a node instance with external pins; and
* a graph asset containing internal nodes and connections.

Containers are nestable. A child graph may contain another Composite node to any
depth allowed by the project safety limit.

### 2.2 Creating a container

The editor supports “Create Container from Selection.” The operation:

1. Creates a new Composite node and child graph.
2. Moves the selected nodes and all wholly internal connections into the child
   graph.
3. Replaces each selected external input connection with a named container
   input pin and an internal input boundary pin.
4. Replaces each selected external output connection with a named container
   output pin and an internal output boundary pin.
5. Reconnects the parent graph to the new Composite node.
6. Preserves node IDs, comments, editor positions, and connection metadata.

The operation must be undoable as one transaction.

### 2.3 Container interface

The Composite interface is explicit and versioned. Each boundary pin has:

* stable pin ID;
* name and display label;
* Data or Exec kind;
* DataType for Data pins;
* direction;
* default value where appropriate;
* optional description and exposed category.

The external node pins are not inferred from screen position. They are stored as
interface records and map to internal boundary pins by stable IDs.

The interface can be edited while the container is open. Renaming a pin does
not break links. Changing pin type requires validation and presents affected
parent connections for repair; it must not silently coerce values.

### 2.4 Collapse and expand

Collapsed mode displays the Composite as one ordinary node with its exposed
inputs and outputs. Double-clicking or an explicit Open command enters the child
graph. The editor must show the breadcrumb path, for example:

`Character Behavior > Locomotion > Grounded Movement`

Expanding a container does not clone or regenerate its contents. The editor
opens the authoritative child graph owned by that Composite instance/asset.

### 2.5 Nesting and execution

When a Composite executes, its input values enter the child graph through the
matching boundary pins. The child graph runs normally, including branches,
sequences, loops, comments, and nested Composite nodes. Child outputs return to
the parent through the matching output boundary pins.

Exec entry and exit are explicit. A container may expose multiple Exec inputs
and outputs, allowing a child Branch or loop to remain structurally meaningful
when collapsed.

The FRust compiler lowers a Composite as a scoped function/block with explicit
arguments and results. Names are scoped to the container path, and nested
containers do not leak locals into their parent. Recursion is rejected unless a
future explicit recursion feature enables it with a depth limit.

### 2.6 Asset and serialization model

A Composite may be:

* inline: child graph stored inside the owning parent graph; or
* reusable: child graph stored as a separate graph asset referenced by the
  Composite node.

The first implementation should support inline containers and use stable graph
and node IDs. The asset reference form follows once duplication and reuse are
needed. Both forms must preserve:

* container identity and interface version;
* child graph schema version;
* parent/child ownership;
* comments and annotation links;
* editor layout for collapsed and expanded views.

Save/load/save must be byte-stable for unchanged containers. Missing child graph
data or an invalid interface mapping is a load error, not an empty container.

### 2.7 Validation

Container validation checks:

1. Every exposed pin maps to exactly one compatible internal boundary pin.
2. Every internal boundary pin is either exposed or intentionally private.
3. Parent connections match the external interface.
4. Exec and Data connections remain separate.
5. Nested depth stays below the project limit.
6. A container cannot directly or indirectly contain itself.
7. Child control-flow validation completes before parent compilation.
8. Interface changes identify all affected parent links.

Validation errors identify the full breadcrumb path and pin names.

## 3. Acceptance tests

The authoring feature is complete only when these cases pass:

1. Attach a comment to a Branch and verify it has no runtime effect.
2. Save/load a graph with comments and verify attachment identity.
3. Convert a selected dataflow chain into a container and preserve results.
4. Convert an Exec branch into a container with two Exec outputs.
5. Collapse, reopen, edit, save, and reload a container without cloning nodes.
6. Nest a container inside a container and compile the parent through FRust.
7. Reject an incompatible interface pin change with a repair diagnostic.
8. Reject self-containing and over-depth containers safely.
9. Preserve comments and editor positions through container creation and undo.
10. Generate FRust with scoped names for nested containers and no leaked locals.

## 4. Delivery order

1. Add Annotation connections and Comment serialization.
2. Add Composite child-graph ownership and explicit interfaces.
3. Add create-from-selection, collapse/expand, breadcrumbs, and undo.
4. Add nested validation and graph serialization tests.
5. Add FRust scoped lowering for Composite graphs.
