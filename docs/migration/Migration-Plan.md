# Creation Suite Migration Plan

## Phase 1: Umbrella Repo

Create the `Creation Suite` repository and link the four app repos into
`apps/` as submodules.

Status on July 28, 2026:

- completed locally

## Phase 2: Define Shared Boundaries

Before moving major code, define what belongs in `shared/` and what
stays in app repos.

Move only genuinely cross-app code into shared modules.

## Phase 3: Shared Asset System

Create `shared/AssetSystem/` and migrate:

- shared asset types
- suite bootstrap/discovery settings
- zip-backed VFS
- asset materialization rules

Then update:

- Creation Station to consume shared asset/VFS code
- Creation Engine to consume the same shared asset/VFS code

## Phase 4: Shared CEL Core

Move CEL core infrastructure into `shared/CEL/`:

- parser/frontend
- semantic analysis
- runtime/JIT core
- common host ABI rules
- common node/code bridge

Keep per-app domain intrinsics outside shared core.

## Phase 5: Shared Build Infrastructure

Move common CMake/LLVM/dependency helpers into `shared/CMake/`.

## Phase 6: Shared Interop Layer

Add `shared/Interop/` for:

- project registry
- cross-app asset references
- compatibility/version rules
- import/export contracts

## Phase 7: App Refactors

Recommended order:

1. Creation Station
2. Creation Engine
3. Creation Movie
4. Creation Live

## Guiding Rule

Do not attempt a giant one-shot merge. Migrate one shared foundation at a
time and keep every app buildable throughout.
