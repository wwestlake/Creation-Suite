# Creation Suite Platform Architecture

Date: July 28, 2026

## Core Correction

The Suite problem is **not** primarily "find duplicated code and move it."

The real problem is:

- define the **shared systems**
- implement them once at the Suite level
- make every app depend on them
- stop allowing each app to invent its own persistence, asset handling, and filesystem behavior

This document is the corrected architecture statement.

## Non-Negotiable Rule

After the user chooses Suite-level storage locations, the apps do **not** treat Windows folders as their project model.

Windows folders are only:

- bootstrap locations
- cache/materialization locations
- export destinations
- external import sources

They are **not** the working project model.

The working project model is:

- Suite VFS
- project containers inside the Suite VFS
- shared asset identity and asset resolution inside that VFS

## Platform Principles

### 1. Projects Are Containers, Not Loose Folders

Each project must be represented by a Suite-managed container.

That container is the project.

It is not:

- a JSON file beside an `_Assets` folder
- an app-specific save file plus loose media nearby
- a Windows directory the app treats as its native data model

The container may physically be stored as:

- a zip-backed archive
- another package format later if needed

But logically it is always:

- a mounted project VFS

### 2. The Suite Owns Storage Semantics

Apps do not decide:

- where project files live
- how assets are copied
- what folder layout means
- how versions are tracked
- how assets are referenced

The Suite platform decides that.

Apps only ask for:

- a project handle
- an asset handle
- a stream
- a typed blob
- a temp materialization when absolutely required

### 3. Assets Are Shared First-Class Objects

Assets are not just filenames.

Assets must have:

- stable asset IDs
- version IDs
- source provenance
- type metadata
- app/domain tags
- compatibility rules
- immutable original references where required

An asset may be:

- audio
- MIDI
- rendered audio
- signal patch data
- tracker clip data
- Foley event data
- plugin preset data
- CEL source
- CEL compiled artifact
- node graph
- video
- still image
- texture
- model
- stream configuration
- render output
- metadata blob

The platform must not assume only files that humans would traditionally call "media files."

### 4. Originals Must Never Be Destroyed

The platform must preserve the artist's original material.

That means:

- imported originals remain identifiable
- derived assets do not silently overwrite originals
- "save" over an existing asset should create a new version or a new named asset according to user intent
- replacement must be deliberate

The asset system must distinguish:

- original source asset
- managed canonical asset
- derived render
- transient work product
- exported artifact

### 5. Apps Are Domain Clients Of The Platform

Creation Station, Creation Engine, Creation Movie, and Creation Live should be thought of as clients of the Suite platform.

They should not each own separate infrastructure for:

- storage
- project serialization
- asset identity
- cross-app references
- provider/account configuration
- interop contracts

## Required Shared Systems

These are the actual systems that must exist before app work is considered structurally safe.

### 1. Suite Storage Bootstrap

Purpose:

- store the user-selected top-level Suite locations
- define where the Suite keeps containers, caches, exports, and temporary materializations

This is the only place ordinary Windows folder selection belongs.

Needed:

- Suite home root
- project container root
- shared resources root
- cache/temp/materialization root
- export root
- optional external content roots

### 2. Suite VFS Core

Purpose:

- provide the canonical storage abstraction for all apps

This must do more than read zip entries.

Needed:

- mount/unmount container
- create new container
- read stream
- write stream
- replace entry
- enumerate entries
- typed metadata access
- transactional safe-save
- crash-safe write strategy
- temp extraction/materialization
- copy-in from external files
- import from streams
- export to external files

Without this, the Suite does not yet have a real platform.

### 3. Project Container System

Purpose:

- define what a project is

Each project container needs:

- manifest
- app domain identifier
- project metadata
- asset catalog
- dependency map
- revision metadata
- creation/modification timestamps
- licensing/subscription markers if needed

This must be Suite-standard, not app-specific.

### 4. Shared Asset Catalog And Resolver

Purpose:

- make assets addressable and reusable across all apps

Needed:

- asset registration
- asset lookup by ID
- version lookup
- latest-compatible resolution
- dependency tracking
- usage reference tracking
- rename semantics
- clone/fork semantics
- replace/new-version semantics

This is the heart of the cross-app workflow.

### 5. Shared Typed Asset IO Layer

Purpose:

- support different storage patterns for different component needs

Some components need:

- byte stream access
- structured document access
- random-access media reads
- in-memory typed object reads
- temp filesystem materialization for third-party tools

The platform must support all of those through one shared contract.

That means the VFS and asset system must expose:

- raw stream interface
- buffer interface
- structured serialization interface
- materialized-file lease interface

### 6. Asset Provenance And Derivation Graph

Purpose:

- preserve the truth of where creative work came from

Needed:

- original source lineage
- derived-from relationships
- render provenance
- transform history markers
- app-of-origin
- tool-of-origin

This is what protects the artist's originals and makes cross-app reuse trustworthy.

### 7. Shared Project Model

Purpose:

- let apps share work without ambiguity

A project is a VFS container, not an app-owned silo. Any app can open any project — opening is opening, not importing. Any app can store its own assets into a project it has open; any other app with that same project open, and that understands the asset's kind, can see and use it. There is no import/export step between suite apps for this — see `docs/architecture/Suite-Shared-Project-Model.md` for the full model and the real open problem it creates (multiple app processes with the same project open at once, and how writes get coordinated).

("Import/export" still applies to interchange with tools *outside* the suite — glTF, third-party DCC formats — see section 8. It does not apply between Creation Suite apps.)

Needed:

- shared project registry (discovery: which projects exist, which domain they originated in)
- cross-app asset reference rules (kind-based capability: an app uses what it understands)
- compatibility rules
- domain capability descriptors
- multi-process write coordination for one project open in more than one app at once (open problem, see the shared project model doc)

Examples:

- Creation Station and Creation Movie have the same project open at once; a sound recorded in Station appears in Movie's view of that project without any export/import action
- Creation Engine renders into a project; any other app with that project open that understands the render's asset kind can use it directly
- CEL code shared as a Suite-level artifact that multiple apps can compile under their own domain policies

### 8. Shared Declarative 3D Scene Asset System

Purpose:

- give the suite one native way to describe editable 3D objects, scene fragments, procedural assemblies, and AI-authored spatial content

This should not be Engine-only. It should be a Suite-level asset family that can be consumed by:

- Creation Engine for realtime world/entity rendering
- Creation Movie for previz, compositing, titles, motion graphics, and shot assets
- Creation Live for virtual sets, overlays, reactive stage elements, and broadcast scenes
- Creation Station where spatial/reactive visualization workflows need shared scene assets

The source form should be human-authorable and AI-authorable, using a declarative schema such as JSON or CEL-backed structure definitions.

The platform should treat this as a real asset pipeline:

- source description preserved as authored truth
- canonical suite scene/model IR normalized for all apps
- derived runtime products cached per app or export target

Needed:

- schema versioning
- stable node IDs
- hierarchy
- transforms
- primitive and external mesh references
- material definitions
- metadata and semantic tags
- provenance and prompt/source tracking
- app-neutral coordinate/unit conventions
- asset references into the Suite VFS
- placeholders for animation, rigging, physics, collision, cameras, and lights

This gives the Suite a native procedural/spatial asset type rather than relying only on interchange formats.

Interchange formats such as glTF still matter, but they should be import/export targets rather than the only editable truth.

### 9. Shared CEL Core

Purpose:

- one language system for the whole suite

Shared core should include:

- lexer/parser/frontend
- AST/sema
- runtime/JIT core
- common host ABI
- common codegen contracts
- shared node bridge/code generation core

Per-app layers should include only:

- domain intrinsics
- domain safety policy
- app capability gating

### 10. Shared Node System Core

Purpose:

- one graph model across the suite where appropriate

Shared node infrastructure should include:

- graph model
- node/pin model
- type registry
- serialization
- analysis
- CEL generation bridge where relevant

Per-app node palettes and domain nodes stay app-specific.

### 11. Shared Suite Services Layer

Purpose:

- host cross-app operational services

This includes:

- BYOK provider catalog
- provider/account configuration
- persisted suite AI account store
- per-app account and model override routing
- secure key storage strategy
- model routing metadata
- request policy and throttling
- subscription/license state
- EULA state
- account/profile data

### 12. Shared Suite UI Shell

Purpose:

- present the common frame every app lives inside

This should include:

- header shell
- account/profile area
- suite settings entry point
- suite storage and VFS control surface
- suite AI account and per-app routing control surface
- shared EULA access panel
- standardized transport framework where relevant
- domain tab shell
- suite settings window
- EULA access

This is not just visual consistency. It is a shared operating surface.

### 13. Shared Materialization And External Tool Bridge

Purpose:

- handle the cases where external tools or plugins require real OS files

Needed:

- temp extraction leases
- cleanup policy
- read-only vs writable materialization
- reconciliation back into the VFS

This is especially important for:

- VSTs
- sample libraries
- codecs
- video tools
- third-party import/export bridges

## What Apps Must Not Do

Once this platform exists, apps must not:

- save loose companion asset folders beside project files
- invent app-local project formats as the primary truth
- directly depend on Windows folder structure for working state
- maintain private asset identity rules
- silently overwrite imported originals
- implement separate BYOK/account/config models
- hardcode their own interop assumptions

## Current Gap Against This Architecture

The current codebase is missing most of this platform in complete form.

The biggest missing pieces are:

- full Suite VFS write/update/materialization support
- true project container model
- true shared asset catalog/resolver
- provenance/derivation tracking
- cross-app interop runtime
- migrated shared CEL core
- migrated shared node core
- complete suite services layer

## Implementation Order

To get infrastructure operational before more application growth:

### Phase A. Storage And Project Truth

Build first:

1. Suite bootstrap settings model
2. Suite VFS core
3. project container format
4. asset catalog/resolver
5. materialization bridge

Until this is done, app persistence work should be considered provisional.

### Phase B. Shared Language And Graph Truth

Build next:

1. shared CEL core extracted from Creation Engine
2. shared NodeSystem core extracted from Creation Engine
3. app policy layers on top

### Phase C. Shared Interop And Services

Build next:

1. project registry
2. cross-app compatibility contracts
3. AI/BYOK shared runtime
4. licensing/subscription service
5. suite UI shell completion

### Phase D. Rewire Every App

Only after those shared systems exist:

- move Station onto them
- move Engine onto them
- move Movie onto them
- move Live onto them

## Canonical Statement

The Creation Suite is not a loose family of apps that happen to share some widgets.

It is a platform with:

- one storage model
- one project model
- one asset identity model
- one shared language core
- one shared graph core
- one shared services/configuration layer

The apps are domain clients of that platform.

That is the architecture to build now.
