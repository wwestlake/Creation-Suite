# Creation Suite Architecture

## Model

The suite consists of six separate applications:

- Creation Station
- Creation Engine
- Creation Movie
- Creation Live
- Creation Modeler
- Creation Texture

Creation Station, Creation Engine, Creation Movie, and Creation Live are
separate app repositories, linked into the umbrella repo as git submodules.
Those product repositories remain independent.

Creation Modeler and Creation Texture currently live as plain directories
under `apps/` and are not yet registered as git submodules.

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

The suite uses a single, suite-wide VFS as the one project/asset store (this
replaces the earlier "two levels of storage" framing below it used to have —
there is one level: the suite VFS):

- Exactly one VFS root, one user-configured location, shared by every app.
  Recommended to live on a large storage device, chosen during first-run
  setup.
- Projects are objects inside that one VFS, not separate per-app project
  roots. Any app can open any project; there is no per-app project silo and
  no import/export step between apps for data that already lives in the
  suite (see [[Suite-Shared-Project-Model]]).
- A background service, not any app process, owns the VFS exclusively and
  exposes it to every app over a localhost HTTP + WebSocket API (see
  [[Suite-Shared-Project-Model]] for the chosen mechanism).
- Suite-level bootstrap configuration (where the VFS root is, whether setup
  has been completed) is itself suite-shared, not app-local: an app will not
  load its main screen until suite setup — VFS location foremost — is
  complete, and that setup flow is a single shared component every app
  defers to, not six separate copies of it.

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
