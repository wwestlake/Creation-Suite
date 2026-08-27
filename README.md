# Creation Suite

Creation Suite is the umbrella workspace for the core LagDaemon creative
applications and their shared development tools:

- Creation Station
- Creation Engine
- Creation Movie
- Creation Live
- Creation Modeler
- Creation Texture
- Creation Engineer
- Creation Developer

Companion clients, including Creation Remote Android and Creation Remote
Receiver, live alongside these applications.

This repository exists to coordinate:

- suite-wide architecture
- shared libraries
- shared build infrastructure
- shared CEL/runtime infrastructure
- shared asset and virtual filesystem infrastructure
- cross-application interoperability standards

## Layout

```text
Creation Suite/
  apps/
    CreationStation/   # git submodule
    CreationEngine/    # git submodule
    CreationMovie/     # git submodule
    CreationLive/      # git submodule
    CreationModeler/
    CreationTexture/
    CreationEngineer/
    CreationDeveloper/
  shared/
    AssetSystem/
    CEL/
    NodeSystem/
    Interop/
    CMake/
    UI/
  docs/
    architecture/
    standards/
    migration/
```

## Intent

The app repositories remain separate products with their own release histories.
This umbrella repo is the coordination layer where shared code lives and where
suite-wide migration work is planned and executed.

## Current Direction

The first shared foundation is the suite asset/VFS stack:

- one suite-level configuration and discovery system
- one shared asset identity model
- one shared zip-backed virtual filesystem model
- per-app domain adapters layered on top

Creation Developer is the Suite environment for building FRust-based Suite
plugins. It uses the same shared shell, Project Manager, and active-project
session as the other applications. Opening a project selects the current VFS
project for the application; individual tools then decide which assets and
working areas they use inside it.

Creation Developer currently provides three distinct terminal surfaces:

- **OS Terminal**: a PowerShell session that operates on the user's machine.
- **FRust Terminal**: the `fr->` language REPL for evaluating FRust code.
- **Frate VFS Terminal**: the `frate>` project terminal for creating and
  navigating Suite plugin pods in the active project's VFS workspace.

For example, `new pod my_plugin` in the Frate VFS Terminal creates a
plugin-pod scaffold in the active Suite project. It does not create a
filesystem project beside the application executable.

See:

- [Suite Architecture](docs/architecture/Suite-Architecture.md)
- [Suite Asset/VFS Standard](docs/standards/Suite-Asset-VFS-Standard.md)
- [Migration Plan](docs/migration/Migration-Plan.md)
- [Station/Engine First Move Checklist](docs/migration/Station-Engine-First-Moves.md)
- [Suite Shared Project Model](docs/architecture/Suite-Shared-Project-Model.md)
- [Multi-Agent Workflow](docs/MULTI_AGENT_WORKFLOW.md)
