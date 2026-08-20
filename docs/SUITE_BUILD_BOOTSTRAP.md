# Suite Build Bootstrap

This document defines the repeatable build setup for a fresh Creation Suite checkout in any agent workspace.

## Goals

- A fresh pull in `CreationSuite-Codex`, `CreationSuite-Claude`, or `CreationSuite-Gemini` configures the same way.
- App builds do not depend on hard-coded `codex` or `claude` paths.
- Machine-level prerequisites are explicit.
- One root command can configure and build the suite.

## Required External Prerequisites

Set these before configuring:

- `JUCE_DIR`
  - Required.
  - Must point at the root of a JUCE checkout.

Optional but commonly needed:

- `CE_WINFLEXBISON_DIR`
  - Directory containing `win_bison.exe` and `win_flex.exe`.
  - Required for fresh CEL grammar generation when configuring apps that consume shared CEL.

- `CE_LLVM_VCPKG_DIR`
  - Path to a `vcpkg_installed/x64-windows` tree containing the suite LLVM build.
  - Usually not needed if `apps/CreationEngine/vcpkg_installed/x64-windows` already exists in the checkout.

- `CREATION_STATION_ASIO_SDK_DIR`
  - Required only for ASIO-enabled Creation Station builds.
  - Must point at the directory containing `iasiodrv.h`.

Optional workspace overrides:

- `CREATION_SUITE_WORKSPACES_ROOT`
  - Overrides the auto-detected workspace root that contains the agent repos and shared `*-debug-bin` folders.

- `CREATION_SUITE_AGENT_ID`
  - Overrides the auto-detected agent id derived from the repo folder name, for example `codex`, `claude`, or `gemini`.

## Fresh Checkout Steps

1. Clone the umbrella repo into your agent workspace.
2. Initialize submodules:

```powershell
git submodule update --init --recursive
```

3. Ensure the LLVM install exists where the suite expects it:

- preferred: `apps/CreationEngine/vcpkg_installed/x64-windows`
- or set `CE_LLVM_VCPKG_DIR` explicitly

4. Set `JUCE_DIR`, and set the optional variables above if your machine needs them.

## Standard Build Entry Point

From the repo root:

```powershell
.\scripts\Build-Suite.ps1 -Configuration Debug
```

That configures and builds:

- Creation Engine
- Creation Engineer
- Creation Station
- Creation Texture
- Creation Modeler
- Creation Movie
- Creation Live
- Creation Suite VFS Service

All real app executables are mirrored into the workspace-shared bin folder for the current agent and configuration.

Examples:

```powershell
.\scripts\Build-Suite.ps1 -Configuration Debug
.\scripts\Build-Suite.ps1 -Configuration Debug -Targets station,texture,vfs
.\scripts\Build-Suite.ps1 -Configuration Debug -EnableStationAsio
```

## Manual One-Off Configure/Build

If you want to configure one component without the root script, use explicit `-S/-B` arguments from the repo root. Example:

```powershell
cmake -S .\apps\CreationStation -B .\apps\CreationStation\build -G "Visual Studio 17 2022" -A x64 -DJUCE_DIR=$env:JUCE_DIR -DCREATION_STATION_ENABLE_ASIO=OFF
cmake --build .\apps\CreationStation\build --config Debug
```

## Shared Bin Resolution

The suite now derives the shared bin folder from the workspace and agent id instead of hard-coding `codex`.

Examples:

- `D:\CreationSuite-Workspaces\codex-debug-bin`
- `D:\CreationSuite-Workspaces\claude-debug-bin`
- `D:\CreationSuite-Workspaces\gemini-debug-bin`

The VFS service client also resolves the service executable from:

1. the current executable directory
2. the configured shared bin directory compiled into the build
3. `CREATION_SUITE_SHARED_BIN_DIR` in the environment

## ASIO

Creation Station does not vendor the Steinberg ASIO SDK.

If you want ASIO support, install or clone the SDK separately and set:

- `CREATION_STATION_ASIO_SDK_DIR`

Then build with:

```powershell
.\scripts\Build-Suite.ps1 -Configuration Debug -EnableStationAsio
```

See [apps/CreationStation/docs/ASIO-SDK-Setup.md](/D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationStation/docs/ASIO-SDK-Setup.md) for the Station-specific details.
