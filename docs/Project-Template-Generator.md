# Suite Project Template Generator

This repository now includes a reusable scaffold for future Creation Suite
projects.

Use it when a new suite app needs:

- the shared suite header and transport shell
- suite-wide settings and configuration access
- shared AI account and routing settings
- project registry visibility
- a language-host boundary
- a predictable source/config/tests/deploy layout

## Generator

Run:

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

## What It Creates

The generated project includes:

- `Source/` app shell with shared suite header integration
- `Language/` app-local language policy layer
- `config/` configuration notes and placeholders
- `tests/` test entrypoint placeholders
- `deploy/` packaging notes and placeholders
- `docs/` architecture and capabilities starter docs
- `cmake/` shared LLVM discovery include

## Shared Capabilities Wired In

The shell is prewired for:

- `shared/UI`
- `shared/Services`
- `shared/AssetSystem`
- `shared/Interop`

That gives every new project one baseline for:

- AI provider selection and routing settings
- suite configuration and path management
- shared EULA/settings access
- shared project-container discovery
- transport/header shell consistency

## Important Note

This is a project scaffold, not a finished domain app. The generated code is
intentionally a structured shell where domain-specific features can be added
without re-solving common suite plumbing each time.
