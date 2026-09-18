# Shared Infrastructure Audit

Date: July 28, 2026

## Bottom Line

The Creation Suite shared infrastructure is **not yet fully centralized or operational**.

What exists today is a **partial foundation**, not a completed suite platform.

The main problem is not that nothing was done. The problem is that the system is in a **split state**:

- some shared code now exists under `D:\000 Creation Suite\shared`
- some of the same logic still exists inside app repos
- some critical suite-wide systems are still only placeholders
- the Suite repo itself currently builds shared libraries only, not the whole suite
- there are still multiple copies of app code in play (`D:\000 Creation Station`, `D:\000 Creation Movie`, `D:\000 Creation Engine`, and duplicated app trees under `D:\000 Creation Suite\apps`)

That means the suite architecture is **not yet authoritative**. We still have competing sources of truth.

## What Is Real And Usable Today

These shared resources exist in `D:\000 Creation Suite\shared` and contain actual code:

### 1. Shared Asset Foundation

Path:

- `D:\000 Creation Suite\shared\AssetSystem`

What is real:

- asset identity types
- asset reference modes
- suite settings storage
- a basic zip-mounted virtual file system

Files:

- `include\creation\assets\AssetTypes.h`
- `include\creation\assets\VirtualFileSystem.h`
- `include\creation\suite\SuiteSettings.h`
- `src\AssetTypes.cpp`
- `src\SuiteSettings.cpp`
- `src\VirtualFileSystem.cpp`

Status:

- usable as a **basic support library**
- **not yet a full project container system**
- **not yet a complete suite VFS**

### 2. Shared Suite UI Foundation

Path:

- `D:\000 Creation Suite\shared\UI`

What is real:

- shared top header bar component
- shared suite settings panel component

Files:

- `src\CreationSuiteHeaderBar.cpp`
- `src\SuiteSettingsPanel.cpp`

Status:

- usable
- this is a legitimate shared UI library
- but only part of the broader suite UI surface is centralized

### 3. Shared AI Provider Catalog

Path:

- `D:\000 Creation Suite\shared\Services`

What is real:

- provider catalog metadata for BYOK-related setup

Files:

- `include\creation\services\SuiteAiSettings.h`
- `src\SuiteAiSettings.cpp`

Status:

- usable as static provider metadata
- **not yet a complete suite AI system**

## What Exists But Is Only Scaffolding

These shared modules currently exist in name only:

### 1. Shared FRust

Path:

- `D:\000 Creation Suite\shared\FRust`

Current state:

- `CMakeLists.txt`
- `README.md`

Actual build target:

- `add_library(creation_suite_cel INTERFACE)`

Meaning:

- no parser
- no sema
- no runtime
- no JIT
- no host ABI implementation
- no shared app-policy layer

This is **not operational**.

### 2. Shared Node System

Path:

- `D:\000 Creation Suite\shared\NodeSystem`

Current state:

- `CMakeLists.txt`
- `README.md`

Actual build target:

- `add_library(creation_suite_node_system INTERFACE)`

Meaning:

- no graph model
- no node model
- no serialization
- no type registry

This is **not operational**.

### 3. Shared Interop

Path:

- `D:\000 Creation Suite\shared\Interop`

Current state:

- `CMakeLists.txt`
- `README.md`

Actual build target:

- `add_library(creation_suite_interop INTERFACE)`

Meaning:

- no suite registry
- no compatibility resolver
- no asset-version negotiation
- no project discovery layer
- no cross-app exchange contracts in code

This is **not operational**.

## What Is Still Duplicated

### 1. Suite Settings Logic

Still duplicated:

- `D:\000 Creation Suite\shared\AssetSystem\src\SuiteSettings.cpp`
- `D:\000 Creation Suite\apps\CreationStation\Source\Suite\SuiteSettings.cpp`

This should have one implementation only.

### 2. Suite Settings Panel

Still duplicated:

- `D:\000 Creation Suite\shared\UI\src\SuiteSettingsPanel.cpp`
- `D:\000 Creation Suite\apps\CreationStation\Source\Views\SuiteSettingsPanel.cpp`

These are clearly parallel versions, not one authoritative shared control.

### 3. Virtual File System

Still duplicated:

- `D:\000 Creation Suite\shared\AssetSystem\src\VirtualFileSystem.cpp`
- `D:\000 Creation Suite\apps\CreationStation\AssetSystem\src\VirtualFileSystem.cpp`
- `D:\000 Creation Engine\AssetSystem\src\VirtualFileSystem.cpp`

The shared and Station versions are effectively the same family.
The Engine version is separate naming/style and still not unified with the shared suite module.

### 4. LLVM Discovery Helper

Duplicated copies exist in app repos:

- `D:\000 Creation Suite\apps\CreationStation\cmake\CreationSharedLLVM.cmake`
- `D:\000 Creation Suite\apps\CreationMovie\cmake\CreationSharedLLVM.cmake`
- `D:\000 Creation Suite\apps\CreationLive\cmake\CreationSharedLLVM.cmake`

These files currently hash identically, which proves they are duplicated shared infrastructure that should live once in the Suite layer.

### 5. App Repositories Themselves

There are currently two structural realities in parallel:

- standalone app repos:
  - `D:\000 Creation Station`
  - `D:\000 Creation Movie`
  - `D:\000 Creation Engine`
  - `D:\000 Creation Live`
- duplicate app repos under:
  - `D:\000 Creation Suite\apps\...`

This is one of the biggest current architecture risks.

If the Suite repo is intended to become the system-of-systems home, then the app relationship must be made authoritative and explicit:

- submodules
- subtree imports
- orchestrated sibling repos
- or another declared model

Right now it is ambiguous in practice.

## What Is Missing Before The Infrastructure Can Be Called Operational

### 1. A Real Suite Project Container Standard

Not done yet.

Needed:

- one actual project archive/container format
- internal directory conventions
- manifest format
- asset index
- version metadata
- change-safe write rules
- recovery strategy
- temporary materialization rules for third-party plugins that need real files

Today the suite does **not** have this.

### 2. A Real Suite VFS

Not done yet.

The current shared VFS can mount and read zip entries, but that is only a small piece of the full requirement.

Still needed:

- write support
- project creation/update flows
- manifest/index writes
- transactional or safe-save behavior
- in-place asset update/version behavior
- temp extraction/materialization
- registry/discovery integration
- app-facing service API

### 3. Shared FRust From Creation Engine

Not moved yet.

The most complete FRust implementation is still in:

- `D:\000 Creation Suite\apps\CreationEngine\Language`

This must become suite infrastructure, with app-specific policy layered above it.

Still needed:

- move parser/frontend
- move sema
- move JIT/runtime core
- move common ABI rules
- move shared node bridge/core generation pieces
- keep app-domain intrinsics outside the shared core

### 4. Shared Node System From Creation Engine

Not moved yet.

The strongest node foundation is still in:

- `D:\000 Creation Engine\NodeSystem`
- `D:\000 Creation Suite\apps\CreationEngine\NodeSystem`

This must become the shared suite node layer before app-specific node editors continue to grow independently.

### 5. Shared Interop Runtime

Not implemented yet.

Needed:

- suite-level registry of app roots and projects (discovery, not ownership -- any app can open any project)
- cross-app asset reference rules (kind-based capability, not import/export -- see docs/architecture/Suite-Shared-Project-Model.md)
- compatibility/version policies
- multi-process write coordination for one project open in more than one app at once
- durable IDs and version resolution rules

### 6. Shared AI/BYOK Runtime

Not implemented yet.

Today there is a provider catalog, persisted suite AI account storage, and per-app account/model routing foundation.

Still needed:

- secure key storage strategy
- multi-provider routing policy beyond the current shared account store
- throttling/retry policy
- model capability metadata
- shared request abstraction
- per-app capability gating

### 7. Shared Suite Configuration Model

Not complete yet.

Current suite config covers:

- suite paths
- app project roots
- suite AI account storage
- per-app AI account/model routing
- licensing/EULA presentation and acceptance persistence

Still needed:

- asset subscription/tier state
- account identity/profile data
- shared content roots
- temp/cache roots
- plugin integration policies

### 8. Shared Transport / Control Surface Layer

Not centralized yet.

The transport/header visual layer has begun to centralize, but the deeper control-surface and app transport behavior is still app-local.

Still needed:

- shared transport semantics
- shared control-surface action registry
- shared MIDI learn mapping model
- common scrub/jog/record/play abstractions

### 9. Shared Branding / Header / Menu System

Partially centralized only.

The header bar exists, but not the full suite shell model:

- common top app frame
- suite menu behavior
- standardized domain-tab structure
- reusable account/profile shell
- app-level menu conventions

## What Must Happen Next

Before more serious application construction continues, the suite needs a focused infrastructure phase.

### Phase 1. Declare The Source Of Truth

Decide and enforce one development model:

- the Suite repo is the orchestration root and apps are attached explicitly
- or the standalone app repos remain primary and Suite shared libs are consumed from one stable shared path

But it cannot remain half-and-half.

### Phase 2. Finish The Shared Core Libraries

Build these for real inside `D:\000 Creation Suite\shared`:

1. `AssetSystem`
   - complete project container model
   - complete VFS API
   - asset catalog/index
   - write/update/materialize support
2. `FRust`
   - migrate the real Engine language core
3. `NodeSystem`
   - migrate the real Engine node core
4. `Interop`
   - implement registry, compatibility, and exchange rules
5. `Services`
   - expand into real suite AI/BYOK/account/licensing services
6. `UI`
   - expand into the reusable suite shell layer

### Phase 3. Remove Duplicates

After the shared versions exist, remove or replace app-local copies of:

- suite settings storage
- suite settings UI
- VFS helpers
- LLVM helper glue where appropriate
- app shell/header clones

### Phase 4. Rewire Apps To Consume Shared Libraries Only

Each app should consume shared libraries from the Suite layer rather than owning its own versions.

That includes:

- Creation Station
- Creation Engine
- Creation Movie
- Creation Live

### Phase 5. Only Then Resume Feature Construction

Once the suite core is authoritative, then application work can continue without creating more structural debt.

## Current Truth Statement

As of July 28, 2026:

- the Suite shared layer is **started**
- the Suite shared layer is **not finished**
- the Suite shared layer is **not yet the sole source of truth**
- the VFS is **not fully implemented**
- shared FRust is **not migrated**
- shared NodeSystem is **not migrated**
- shared Interop is **not implemented**
- shared AI/BYOK services are **not implemented beyond provider metadata**

So the correct answer is:

**we are not ready to treat the suite infrastructure as complete yet**

and

**the next responsible move is to finish the shared platform before continuing broad app feature work**
