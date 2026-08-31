# Creation Texture Functional Specification

Status: app shell scaffolded and building against the shared platform (`scripts/New-SuiteProject.ps1`, `AppDomain=texture`, `LogoId=texture`); domain design starting with this document. No domain-specific `Source/`/`Language/` code exists yet.

## 1. Overview And Core Philosophy

Creation Texture is the suite's 2D/PBR surface-authoring environment: procedural texture generation, texture painting, and material graph editing. It produces the material and surface assets that [[Creation Modeler]]'s geometry, Creation Engine's realtime renderer, and Creation Movie's compositing pipeline all consume.

Like Creation Modeler, it should not become a general-purpose image editor. Its purpose is narrow and pipeline-focused:

- procedural + hand-authored texture/material creation
- physically-based material definition (albedo, normal, roughness, metallic, AO, height, emissive)
- node-graph procedural generation built on the shared `NodeSystem`/FRust stack, not a bespoke graph engine
- direct, versioned output into the suite asset/VFS system as first-class material assets — not loose PNG/EXR files on disk

## 2. Core Workflows

### 2.1 Procedural Material Graph

Node-based procedural texture/material authoring — noise, patterns, blending, layering, masks — on the shared `NodeSystem` graph model, using the same node-to-FRust generation approach the rest of the suite is standardizing on (see [[System-Architecture]]).

### 2.2 Painting Workspace

Direct raster painting onto UV-mapped surfaces or flat canvases, with layers, for hand-authored detail that a procedural graph alone doesn't cover well.

### 2.3 PBR Map Baking And Channel Packing

Bake the full PBR channel set (albedo, normal, roughness, metallic, AO, height, emissive) from a material definition, and pack channels into engine-ready texture atlases.

### 2.4 Material Library

A browsable, versioned library of material assets — searchable by domain tag, source app, and channel type — consistent with the suite's shared asset catalog rather than an app-local file browser.

## 3. Asset Model

Textures and materials are Suite Asset System first-class objects:

- source material definition (procedural graph + paint layers) preserved as authored truth
- baked/derived texture maps stored as versioned derived assets — baking never overwrites the source definition
- material assets reference-linked into Modeler's surface assignments and Engine's runtime materials, following the same non-destructive-original rule as every other suite asset type (see [[System-Architecture]] §4)

## 4. Relationship To Shared Suite Systems

Built on, not duplicating:

- suite VFS and project container strategy (`shared/AssetSystem`)
- shared asset identity, versioning, and provenance
- shared `NodeSystem`/FRust for procedural graphs — no local graph engine
- shared shell/header/account/suite settings systems — already wired via the project scaffold

## 5. Relationship To Other Suite Apps

- **Creation Modeler** — materials assigned to Modeler surfaces; Texture is the natural authoring counterpart to Modeler's geometry authoring
- **Creation Engine** — baked maps consumed as runtime PBR materials
- **Creation Movie** — texture/material assets usable in compositing, titles, and motion graphics

## 6. Current Implementation State (2026-07-29)

The app shell exists and is wired to `shared/AssetSystem`, `shared/Services`, `shared/UI`, `shared/Interop`; the `texture` domain and logo are already registered in shared code (`ProjectManifest`, `SuiteStoragePaths`, `CreationSuiteLogos`). No domain-specific code exists in `Source/` or `Language/` yet — those are still template stubs. This document is the first pass at defining what the domain actually is. It should be treated as a starting proposal, not a locked decision — the app's own `docs/CAPABILITIES.md` and `docs/ARCHITECTURE.md` (previously bare template placeholders) have been filled in with a condensed version of this spec and point back here for full depth.

## 7. Explicitly Not Decided Yet

- exact node palette for the procedural graph
- painting engine/library choice
- baking pipeline performance targets
- initial project template and startup flow
- whether the standalone app or a plugin form (analogous to Station's runtime-plugin direction) ships first
- GitHub repository / submodule registration — the app currently exists only as a local scaffold under `apps/CreationTexture` with its own local git init; it is not yet a registered submodule of this umbrella repo (no `.gitmodules` entry, no remote configured)
