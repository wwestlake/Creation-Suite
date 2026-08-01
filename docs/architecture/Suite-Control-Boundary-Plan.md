# Suite Control Boundary Plan

## Why this exists

The suite already has real CEL execution, real LLVM JIT, real domain
gating, and a real engine-side simulation loop.

What it does **not** have yet is one explicit authored-control boundary:
the clean layer where authored logic enters a host app, receives time or
events, and is allowed to touch only the capabilities that host exposes.

Without that layer, every next feature request turns into the same hole:
event timeline, automation track, shader/material graph behavior,
cross-app logic reuse, and remote-triggered actions all try to enter the
system through ad hoc app code instead of one stable contract.

This plan defines that contract and the milestones to land it.

## What is already real

- `shared/CEL` now contains the suite-level parser, sema, nodegen,
  JIT/runtime sources, host-ABI helpers, and `celc`.
- `shared/NodeSystem` is already a real suite target.
- `EngineCore` already has the correct low-level runtime seam:
  `ScriptComponent`, `IScriptRuntime`, and `Simulation::Step`.
- domain gating already exists conceptually and mechanically through
  `IntrinsicDomainSet` and the app `AppLanguagePolicy` layers.
- client/server script parity is already proven by the real simulation
  tests.

So the problem is no longer "can CEL run?" The problem is "what is the
official authored control surface of the suite?"

## The boundary

The control boundary is the layer between:

- shared authored logic assets
- host-specific runtime capabilities
- host-specific time/event sources

In plain English:

- CEL stays the shared language core
- nodes stay the shared authored representation where appropriate
- each app exposes a host adapter
- authored logic never talks straight to random app internals
- timelines, automation lanes, graph execution, and remote triggers all
  enter through the same host-facing execution contract

## The model

The suite should treat authored control as five explicit pieces:

### 1. Script module

The authored unit that the suite stores and moves around.

Examples:

- a `.cel` script
- a `.celg` node graph that generates CEL
- later, a timeline-driven event program that still targets CEL entry
  points

This is the portable logic artifact.

### 2. Capability profile

The declared set of domains/actions the module needs.

This is the existing domain-policy idea made first-class:

- compile against an allowed domain set
- reject disallowed calls at compile time
- never register blocked host trampolines at runtime

This is what makes one shared language safe across different apps.

### 3. Host adapter

The app-owned layer that exposes safe functionality to CEL.

Examples:

- Engine world/entity/transform calls
- Station transport/mixer/tracker actions
- Movie timeline/render actions
- Texture material/shader-generation functions

Apps own adapters. The suite owns the language and the rules.

### 4. Trigger source

The thing that invokes authored logic.

Examples:

- simulation tick
- transport tick
- timeline event point
- UI action
- remote-control command
- asset lifecycle event

This is the missing conceptual bridge that keeps causing feature work to
sprawl.

### 5. Execution report

The structured result of authored execution.

Examples:

- success/failure
- diagnostics
- trace events
- state changes requested
- host-side safety refusal

This is what lets the editor, timeline, automation UI, and later remote
surfaces all inspect what happened using one shape.

## First milestone stack

### Milestone 1: Define the suite control boundary contract

Write down the shared contract and freeze the terminology:

- script module
- capability profile
- host adapter
- trigger source
- execution report

Deliverable:

- this architecture plan promoted into the official roadmap/wiki
- exact responsibility split between `shared/` and app repos

Success means the next feature can be placed cleanly before coding.

### Milestone 2: Make suite CEL/NodeSystem authoritative

Finish the cutover already started:

- Engine stops owning the active `Language/` and `NodeSystem/` source of
  truth
- Engine consumes `shared/CEL` and `shared/NodeSystem`
- Engine keeps only its host adapter and app policy

Success means the language core is truly suite-owned.

### Milestone 3: Land the Engine host adapter layer

Build the first fully explicit host adapter around the already-working
Engine runtime seam.

It should formalize:

- what world actions CEL may request
- what world queries CEL may read
- which entry points are lifecycle-driven vs externally triggered
- how faults, traces, and refusals are reported

Success means Engine becomes the clean reference host, not a special
case.

### Milestone 4: Build the real Engine event timeline

After the host adapter exists, the event timeline stops being a rat hole
and becomes a normal trigger source.

The first version should:

- schedule events against engine time
- invoke CEL entry points deterministically
- surface diagnostics and trace information
- run through the same control boundary as simulation-driven scripts

Success means "timeline automation" is no longer a custom side system.

### Milestone 5: Expand the pattern to other domains

Once Engine proves the pattern:

- Station can add transport/automation triggers
- Movie can add timeline/render triggers
- Texture can add pure-function material/shader generation paths

Success means the suite has one language strategy instead of one per app.

## Immediate implementation order

1. Finish the shared CEL/NodeSystem cutover so the suite really owns the
   language core.
2. Define the Engine host adapter contract in code and docs.
3. Build the Engine event timeline on that contract.

That is the real critical path.

Not because the timeline is unimportant, but because without the shared
language authority and host adapter, the timeline has nowhere stable to
plug in.
