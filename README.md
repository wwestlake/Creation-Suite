# Creation Suite

Creation Suite is the umbrella workspace for the four LagDaemon creative applications:

- Creation Station
- Creation Engine
- Creation Movie
- Creation Live

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

See:

- [Suite Architecture](docs/architecture/Suite-Architecture.md)
- [Suite Asset/VFS Standard](docs/standards/Suite-Asset-VFS-Standard.md)
- [Migration Plan](docs/migration/Migration-Plan.md)
- [Station/Engine First Move Checklist](docs/migration/Station-Engine-First-Moves.md)
