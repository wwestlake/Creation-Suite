# Suite Project Scaffold Instructions For Another LLM

This document explains exactly how to create a new Creation Suite project from
the reusable scaffold already stored in this repository.

Use these instructions when the user wants a new suite app such as:

- `Creation Texture`
- `Creation Modeler`
- future suite apps built on the same shared shell

## Goal

Create a new app repository or folder that already includes:

- the shared Creation Suite shell/header
- suite settings and EULA access
- shared AI provider/account routing access
- shared project registry wiring
- app-local language host boundary
- standard `config`, `docs`, `tests`, and `deploy` folders

Do not hand-build the app from scratch unless the generator is broken and the
user explicitly wants manual recovery.

## Source Of Truth

The scaffold source lives here:

- [scripts/New-SuiteProject.ps1](D:/000 Creation Suite/scripts/New-SuiteProject.ps1:1)
- [templates/SuiteProjectShell](D:/000 Creation Suite/templates/SuiteProjectShell)

The generator copies the template tree, replaces tokens, and emits a ready
starter app shell.

## Required Inputs

Before generating a project, determine these values:

1. `ProjectName`
   Example: `Creation Texture`

2. `TargetDirectory`
   CRITICAL RULE: Must ALWAYS be inside `Creation Suite\apps\`!
   Example: `D:\000 Creation Suite\apps\CreationTexture`

3. `AppDomain`
   Allowed values:
   - `unknown`
   - `station`
   - `engine`
   - `movie`
   - `live`
   - `texture`
   - `modeler`

4. `LogoId`
   Allowed values:
   - `suite`
   - `station`
   - `engine`
   - `movie`
   - `live`
   - `texture`
   - `modeler`

Optional values:

- `CompanyName`
  Default: `LagDaemon Software`

- `BundleId`
  If omitted, the generator creates one automatically.

- `SuiteSharedDir`
  Default: `D:/000 Creation Suite/shared`

## Default Mapping Rule

If the new app is one of the planned suite apps and it has a matching shared
logo/domain, use the same token for both `AppDomain` and `LogoId`.

Examples:

- `Creation Texture` -> `AppDomain texture`, `LogoId texture`, `TargetDirectory D:\000 Creation Suite\apps\CreationTexture`
- `Creation Modeler` -> `AppDomain modeler`, `LogoId modeler`, `TargetDirectory D:\000 Creation Suite\apps\CreationModeler`

## Exact Command

Run this command from the suite repo root:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\New-SuiteProject.ps1 `
  -ProjectName "Creation Texture" `
  -TargetDirectory "D:\000 Creation Suite\apps\CreationTexture" `
  -AppDomain texture `
  -LogoId texture
```

Example for the second planned app:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\New-SuiteProject.ps1 `
  -ProjectName "Creation Modeler" `
  -TargetDirectory "D:\000 Creation Suite\apps\CreationModeler" `
  -AppDomain modeler `
  -LogoId modeler
```

If the target directory already exists and the user explicitly wants it
recreated, add:

```powershell
-Force
```

Do not use `-Force` unless the user has clearly approved replacing the target
directory.

## What The Generator Does

The generator:

1. copies `templates/SuiteProjectShell`
2. renames tokenized files and folders
3. replaces template tokens in file contents
4. emits a ready scaffold with suite wiring already in place

Important generated areas:

- `Source/`
- `Language/`
- `config/`
- `deploy/`
- `docs/`
- `tests/`
- `cmake/`

Important generated files:

- `CMakeLists.txt`
- `README.md`
- `AGENTS.md`
- `Source/Main.cpp`
- `Source/MainComponent.cpp`
- `Source/MainComponent.h`
- `Language/AppLanguagePolicy.cpp`
- `Language/AppLanguagePolicy.h`

## Shared Capabilities Already Included

The generated shell already links to:

- `shared/AssetSystem`
- `shared/Services`
- `shared/UI`
- `shared/Interop`

That means the new app already has the base wiring for:

- suite header and transport shell
- suite settings panel
- suite EULA panel
- suite AI settings store and routing resolution
- project registry queries
- shared project container/storage model

## Required Verification After Generation

After running the generator, verify all of the following.

1. The target directory exists.

2. These folders exist:
   - `Source`
   - `Language`
   - `config`
   - `deploy`
   - `docs`
   - `tests`
   - `cmake`

3. These files exist:
   - `CMakeLists.txt`
   - `README.md`
   - `Source/Main.cpp`
   - `Source/MainComponent.cpp`
   - `Language/CMakeLists.txt`

4. No unresolved template tokens remain.

Run:

```powershell
rg -n "__[A-Z_]+__" "D:\path\to\new\project"
```

This command should return no matches.

5. The generated app domain is correct inside the source.

Check:

- `Source/MainComponent.cpp`
- `Language/AppLanguagePolicy.cpp`

6. The generated app name is correct inside:

- `README.md`
- `CMakeLists.txt`
- `Source/Main.cpp`

## When To Edit After Generation

Only make follow-up edits if the user asked for more than the default scaffold.

Examples of valid follow-up edits:

- change branding colors
- add initial domain-specific panels
- add starter docs for the app’s capabilities
- adjust default transport button visibility
- add extra domain tokens to `canRunNodeDomain`

Do not rewrite the shared shell architecture unless the user specifically asks
for a different template design.

## If A New Domain Is Missing

If the user asks for a new suite app whose domain is not yet recognized:

1. update the shared enum in:
   - [ProjectManifest.h](D:/000 Creation Suite/shared/AssetSystem/include/creation/assets/ProjectManifest.h:1)

2. update storage/display token handling in:
   - [ProjectManifest.cpp](D:/000 Creation Suite/shared/AssetSystem/src/ProjectManifest.cpp:1)

3. update project container folder mapping in:
   - [SuiteStoragePaths.cpp](D:/000 Creation Suite/shared/AssetSystem/src/SuiteStoragePaths.cpp:1)

4. update project discovery scanning in:
   - [ProjectRegistry.cpp](D:/000 Creation Suite/shared/Interop/src/ProjectRegistry.cpp:1)

5. confirm a matching shared logo exists in:
   - [CreationSuiteLogos.h](D:/000 Creation Suite/shared/UI/include/creation/ui/CreationSuiteLogos.h:1)

Only after those updates should the new domain be used in the generator.

## What Not To Do

Do not:

- manually copy one of the existing apps unless the generator is unusable
- fork `Creation Movie` or `Creation Live` by hand as the default workflow
- duplicate shared suite code into the new app
- invent a new folder layout when the scaffold already provides one
- use destructive overwrite on an existing project directory without approval
- leave unresolved template tokens in generated files

## Recommended Response Pattern

After generation and verification, report:

1. the project name
2. the generated path
3. that the suite shell was scaffolded successfully
4. whether verification passed
5. any skipped step, such as not running a full CMake build

Example:

`Created the Creation Texture scaffold at D:\000 Creation Texture using the suite project generator. Verified the expected folders/files and confirmed there are no unresolved template tokens. I did not run a full JUCE/CMake build yet.`
