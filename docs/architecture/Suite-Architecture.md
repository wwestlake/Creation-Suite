# Creation Suite Architecture

## Model

The suite consists of four separate applications and four separate app
repositories:

- Creation Station
- Creation Engine
- Creation Movie
- Creation Live

Those product repositories remain independent.

The `Creation Suite` repository is the umbrella workspace and shared-code
home.

## Repository Strategy

- `Creation Suite` owns suite-wide architecture and shared libraries.
- Each app repo remains its own product and release unit.
- The app repos are linked into the umbrella repo as git submodules.
- Shared code moves into `shared/` only when it is truly cross-app.

## Shared Code Categories

Move into `shared/`:

- Asset/VFS infrastructure
- suite discovery/bootstrap configuration
- CEL core language/runtime/JIT infrastructure
- cross-app interoperability contracts
- shared CMake and dependency helpers

Keep inside app repos:

- product-specific UI
- product-specific workflows
- app branding and release packaging
- app-specific domain intrinsics and actions

## Asset And Project Hierarchy

The suite uses two levels of storage:

1. Suite-level shared configuration/discovery
   This is the canonical location every app can find. It stores suite
   configuration, shared resources, and project root discovery data.

2. App-level project spaces
   Each app has its own project root, discovered through the suite
   configuration layer. Projects inside those spaces use the suite asset
   model and virtual filesystem conventions.

## CEL Strategy

CEL core is shared.

Apps do not share the same domain surface.

That means:

- parser/sema/runtime/JIT live in shared code
- shared host ABI rules live in shared code
- each app contributes its own domain intrinsics
- domain policy must block app-incompatible code from compiling or running

This preserves “one language across the suite” without letting game-only
code execute in the DAW or audio-only code execute in the game engine.
