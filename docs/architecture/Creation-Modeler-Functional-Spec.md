# Creation Modeler Functional Specification

Status: planned only, not yet set up for implementation

## 1. Overview And Core Philosophy

Creation Modeler provides a streamlined, native asset authoring environment designed specifically for the Creation Suite declarative rendering pipeline.

It is intentionally not a general-purpose content creation suite. Its purpose is to focus on:

- core structural modeling
- rapid layout
- articulation-friendly scene building
- direct serialization into suite-native scene/model formats
- real-time, engine-accurate preview

All outputs should serialize directly into the suite canonical intermediate representation and versioned VFS-backed asset formats.

This product should be planned only at this stage. Project templates, startup flows, and implementation structure will be decided later.

## 2. Multi-Mode Modeling Paradigms

Creation Modeler should support multiple workspaces tuned for different geometric needs.

### 2.1 Voxel Workspace

Grid-based volumetric sculpting for:

- rapid prototyping
- chunky asset construction
- block-out stages

### 2.2 Free-Form Workspace

Direct geometric editing for:

- vertex manipulation
- edge manipulation
- polygon manipulation
- primitive-based construction
- transform-handle editing

### 2.3 2D Drafting Workspace

Planar authoring mode for:

- profiles
- cross-sections
- heightmaps
- sketches intended for extrusion or procedural generation

## 3. Animation And Articulation

Creation Modeler is not intended to begin as a heavy timeline animation package. Instead, it should focus on structure and articulation.

### 3.1 Transform Gizmos

Provide direct translation, rotation, and scaling handles operating on scene graph nodes.

### 3.2 Joint And Hierarchy Authoring

Provide visual linking of parent-child node structures to define:

- limbs
- hinges
- pivot points
- mechanical articulation

### 3.3 State Preview

Provide real-time preview of articulation limits and kinematic ranges directly in the viewport.

## 4. Texturing And Material Assignment

Creation Modeler should allow direct surface assignment for suite-native rendering materials.

### 4.1 Surface Properties

Support direct assignment of:

- shader parameters
- base color
- roughness
- metallic
- related surface values and maps

### 4.2 Immediate Feedback

Rendering inside Creation Modeler should follow the runtime engine path closely enough to preserve strong visual parity between editor and final result.

## 5. Scene Composition, Layering, And Asset Management

### 5.1 Hierarchical Scene Graph

Provide an outliner showing:

- node trees
- grouping
- transform inheritance

### 5.2 Layer Management

Support:

- visibility toggles
- locking
- organizational grouping

### 5.3 VFS Asset Linkage

Modeler assets must participate natively in the suite asset and VFS strategy so they are:

- versioned
- shareable
- dependency-aware
- usable by multiple apps

## 6. Relationship To Shared Suite Systems

Creation Modeler should not invent its own isolated storage or scene format.

It should be built on top of shared suite foundations:

- suite VFS and project container strategy
- shared declarative 3D scene/model asset system
- shared asset identity and versioning
- shared provenance and derivation tracking
- shared CEL integration where appropriate
- shared shell/header/account/suite settings systems

## 7. Near-Term Planning Notes

## 8. Modular Extension Strategy

Creation Modeler should be built on a modular architecture with an extensible plugin model.

### 8.1 Core Stability

The primary modeling, transform, and scene composition tools should remain lightweight and performant.

The core application should avoid accumulating unrelated specialty systems directly into its base runtime.

### 8.2 Specialized Expansion

Additional capabilities should be introduced as discrete, loadable modules rather than as invasive core changes.

Examples include:

- custom procedural generators
- specialized shaders
- domain-specific export tools
- focused authoring pipelines for particular asset families

### 8.3 Ecosystem Parity

Extensions must plug directly into the suite Virtual File System and canonical intermediate representation pipeline.

That ensures any newly introduced capability immediately participates in the same declarative asset language as the rest of the suite.

This is important for:

- cross-app interoperability
- consistent asset identity and versioning
- non-destructive source preservation
- long-term maintainability of the suite toolchain

At this time, this specification is being documented only.

Explicitly not decided yet:

- project template strategy
- initial repository/app setup
- startup flow
- implementation milestone order
- viewport panel layout details
- exact tool-selection behavior

Those decisions should happen later, after the broader suite platform and shared declarative 3D asset planning are farther along.
