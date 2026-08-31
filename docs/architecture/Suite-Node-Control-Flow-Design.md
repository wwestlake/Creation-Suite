# Suite Node Control-Flow Design

Status: design baseline for review  
Scope: shared Suite NodeSystem and its FRust compiler/runtime integration

## 1. Purpose

The Suite NodeSystem is a general visual execution-graph system. It must be
able to express decisions, ordered actions, repetition, and early exits without
depending on a particular host application or programming language.

Control flow is not an optional collection of convenience nodes. It is the
mechanism that determines which parts of a graph execute. A graph that can only
evaluate values in a fixed topological order is a dataflow graph, not a complete
behavior or automation system.

The shared NodeSystem owns the graph representation, pin contracts,
serialization, validation, and execution semantics. FRust is the one language
backend and runtime integration for this system. The Suite layer does not
maintain a collection of interchangeable backends.

## 2. Graph model

Every node has two independent connection domains:

* Data pins carry typed values and are evaluated only when the owning node is
  reached by active execution.
* Exec pins carry control tokens. An exec output selects the next node or nodes
  to run; it is not a data dependency and must not be included in data
  topological sorting.

An execution graph may contain cycles only when the cycle is owned by an
explicit loop construct. Accidental cycles remain validation errors. This
requires replacing the current rule of “all exec cycles are invalid” with
structured control-flow validation.

## 3. Core node contracts

The first generic catalog contains the following nodes. Names are stable asset
identifiers; display labels may be localized later.

### Branch — `core.branch`

Inputs: `execute` (Exec), `condition` (Bool)  
Outputs: `true` (Exec), `false` (Exec)

When `execute` arrives, `condition` is evaluated exactly once. Only the
corresponding output fires. An unconnected branch output means execution ends
at that branch; it does not implicitly execute both paths.

### Sequence — `core.sequence`

Input: `execute` (Exec)  
Outputs: ordered Exec outputs, initially `then_0` and `then_1`

Outputs fire in order. Sequence is the standard way to express multiple actions
after one event and is preferable to relying on arbitrary wire ordering.

### For — `core.for`

Inputs: `execute` (Exec), `firstIndex` (Int), `lastIndex` (Int), `step` (Int)  
Outputs: `body` (Exec), `completed` (Exec), `index` (Int)

The loop enters with a deterministic integer range. For each valid index it
publishes `index` and fires `body` once. When the range is exhausted it fires
`completed` once. A zero step is invalid. The default range is bounded and the
runtime applies a maximum-iteration guard.

### While — `core.while`

Inputs: `execute` (Exec), `condition` (Bool)  
Outputs: `body` (Exec), `completed` (Exec)

The condition is evaluated before every body execution, including the first.
False immediately fires `completed`. True fires `body`; the loop returns to the
condition only after the body path finishes. A per-invocation iteration limit
prevents an accidental infinite loop from locking the host.

The condition must be reevaluated by the loop, not latched when the node is
entered. A condition that never changes is therefore a valid graph but will
terminate through the configured safety limit with a diagnostic.

### Break — `core.break`

Input: `execute` (Exec)  
Output: none

Break terminates the nearest enclosing loop and resumes at that loop's
`completed` output. Break outside a loop is a validation error.

### Continue — `core.continue`

Input: `execute` (Exec)  
Output: none

Continue skips the remainder of the current loop body and begins the next
iteration. Continue outside a loop is a validation error.

### Return — `core.return`

Input: `execute` (Exec), optional typed result pins  
Output: none

Return terminates the current behavior/function invocation. It is distinct from
Break, which only exits one loop.

## 4. Structured regions

Control-flow nodes define regions, rather than being treated as ordinary
single-input/single-output nodes:

* A Branch owns two sibling regions: true and false.
* A For and While own one body region and one completion continuation.
* Break and Continue must resolve to the nearest enclosing loop region.
* Return resolves to the current graph/function boundary.

The graph format must preserve enough information to identify region ownership.
Wire shape alone is insufficient: a wire can show that two nodes are connected,
but not whether a cycle is a legal loop, which loop a Break belongs to, or which
branch a node belongs to after editing.

The initial implementation may derive regions from explicit control-flow pins
and a validated entry graph. If that proves ambiguous for nested loops or graph
editing, add a serialized `regionId`/`parentRegionId` field rather than relying
on screen position or node insertion order.

## 5. Validation rules

Validation happens before compilation and before runtime execution.

1. Every control pin connects only to a compatible control pin.
2. A single Exec input accepts at most one incoming connection.
3. Branch has exactly one Exec input and exactly two Exec outputs.
4. Loop body and completion paths are structurally identifiable.
5. Break and Continue resolve to an enclosing loop.
6. Data dependencies used by an active path are type-correct and acyclic unless
   the node explicitly declares a stateful feedback contract.
7. A loop step cannot be zero.
8. Every loop has a configured maximum iteration count, with a project default
   and a per-node override.
9. A graph cannot have an unbounded recursive call path without an explicit
   recursion policy and depth limit.
10. Unreachable nodes are warnings by default, not silent behavior.

The validator should report the node IDs, pin names, and region context in every
error so the editor can select the offending wire/node.

## 6. Execution semantics

Execution is token-based:

1. An event or graph entry creates an execution token.
2. The token enters one node.
3. The node evaluates only the data inputs needed for that execution.
4. The node emits zero or more successor tokens according to its contract.
5. A scheduler processes tokens deterministically.

The first runtime is single-threaded and deterministic. Parallel execution is a
future scheduler feature and must not change graph meaning. Sequence order,
branch selection, loop index order, and completion behavior are observable and
therefore part of the contract.

Each execution invocation carries a safety context:

* maximum total node steps;
* maximum iterations per loop;
* maximum call/recursion depth;
* cancellation flag;
* diagnostic trace sink.

When a guard trips, execution stops with a structured diagnostic identifying the
active node, loop, iteration, and source graph asset. The host remains usable.

## 7. FRust lowering

The shared graph model must remain cleanly separated from the editor UI, but it
is specifically designed to lower into FRust. The compiler receives a validated
structured graph or an equivalent control-flow intermediate representation.

FRust lowering will map:

* Branch to `if/else` blocks;
* Sequence to ordered statements;
* For to a bounded `for`/range construct;
* While to a guarded `while` construct;
* Break and Continue to native loop statements;
* Return to a function return.

FRust lowering must not silently linearize a branch or drop a loop edge. If a
construct is not yet implemented, compilation fails with a precise unsupported
control-flow diagnostic.

The lowering pass should first produce a structured FRust control-flow IR.
Direct “walk the next `then` pin” code generation is insufficient for branches,
nested regions, loops, and multiple exits.

## 8. Serialization and compatibility

The existing graph serialization remains backward compatible:

* Existing nodes and data connections load unchanged.
* New control-flow node types are ordinary registered node types with explicit
  pin signatures.
* The graph schema version increments when region metadata is introduced.
* Loading an unsupported control-flow node produces a clear missing-node error,
  not a partially executable graph.
* Node IDs, pin IDs, connection IDs, and region IDs remain stable across
  save/load/save.

## 9. Editor requirements

The node palette must expose a `Control Flow` category containing Branch,
Sequence, For, While, Break, Continue, and Return. Each node needs:

* a useful display name and description;
* visible Exec pins distinct from Data pins;
* inline safety-limit fields where applicable;
* validation feedback on the node and offending connection;
* a short tooltip describing when the node fires its outputs.

The editor must allow nested loop/branch graphs and make active execution
observable later through debug tracing. A graph that compiles but gives no clue
which path ran is not sufficient for production use.

## 10. Acceptance tests

The shared NodeSystem milestone is complete only when these tests pass:

1. A Branch with true and false sinks executes exactly one sink for each input
   condition.
2. A Sequence executes two and then three actions in declared order.
3. A For loop visits every index in ascending and descending ranges and fires
   completed exactly once.
4. A While loop reevaluates a changing condition and terminates normally.
5. Break exits only the nearest loop; Continue skips only the current iteration.
6. Nested Branch inside a loop and nested loop inside a Branch validate and
   execute correctly.
7. An accidental control cycle is rejected with a precise diagnostic.
8. An unbounded While terminates through the iteration guard without hanging the
   host process.
9. Save/load/save preserves all control pins, connections, and region data.
10. The FRust backend emits and validates real structured source for the branch
    and loop cases; it does not merely report that lowering is unsupported.

## 11. Delivery order

1. Add explicit control-flow descriptors and shared structured-region metadata.
2. Replace unconditional Exec-cycle rejection with legal-region validation.
3. Add deterministic FRust execution semantics and the safety context.
4. Add the core catalog and editor palette entries.
5. Add FRust structured lowering and compile/execute tests.
6. Propagate the stabilized control-flow contracts into the Frust project
   itself, keeping the Suite NodeSystem and FRust implementation aligned.

This document is the completeness gate for the feature. Control flow is not
complete until the acceptance tests and editor requirements are both satisfied.
