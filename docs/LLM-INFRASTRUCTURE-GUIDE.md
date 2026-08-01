# Creation Suite Infrastructure Guide For LLMs

Date: July 29, 2026

This document explains how an LLM should work inside the Creation Suite codebase without reintroducing app-local infrastructure drift.

## Bottom Line

If you are adding or changing shared behavior, prefer `shared/` first.

Do not build new app-local persistence, VFS, suite settings, account/provider settings, shared shell behavior, or project-container logic inside a specific app unless you are intentionally creating a temporary compatibility wrapper around the shared implementation.

## Non-Negotiable Rules

1. Projects are suite-managed containers, not loose folders.
2. Windows folders are bootstrap, cache, export, and import locations only.
3. The suite owns storage semantics, asset identity, materialization, and project-container discovery.
4. Apps may wrap shared infrastructure for app-specific UX, but they must not fork the implementation.
5. New shared functionality belongs in `shared/` unless it is truly domain-specific.

## Where Core Infrastructure Lives

### Shared Asset And Container Platform

Primary location:

- `shared/AssetSystem/`

Key classes:

- `creation::assets::ProjectSession`
- `creation::assets::ProjectContainerService`
- `creation::assets::ProjectAssetService`
- `creation::assets::ProjectWorkspaceService`
- `creation::assets::AssetMaterializer`
- `creation::assets::VirtualFileSystem`
- `creation::suite::SuiteSettingsStore`

Use these when you need:

- create/open/list/find suite project containers
- import assets into a project container
- resolve asset references
- materialize temp files for external tools
- reconcile writable materializations back into the container
- detect stale sessions before overwrite

Do not:

- invent a new per-app archive format
- treat a project folder as the source of truth
- bypass `ProjectWorkspaceService` or `ProjectContainerService` for new suite-level workflows

### Shared UI Shell

Primary location:

- `shared/UI/`

Key classes:

- `creation::ui::CreationSuiteHeaderBar`
- `creation::ui::SuiteShellController`
- `creation::ui::SuiteSettingsPanel`

Use these when you need:

- the top suite header
- suite settings
- shared project browser launch behavior
- shared profile/project/suite actions

Do not:

- rebuild a lookalike top bar in an app
- wire fake or app-local duplicates for suite actions that should be shared

Transport controls are not part of the suite header contract.

If an app needs transport, place it below the shared header.

### Shared Services

Primary location:

- `shared/Services/`

Use this layer for:

- AI account/provider orchestration
- suite logging
- legal/EULA settings
- cross-app service behavior

Do not add new app-local copies of these behaviors unless you are bridging to the shared version during a migration.

### Shared Interop

Primary location:

- `shared/Interop/`

Use this for:

- project discovery
- cross-app project/asset lookup
- future compatibility and reference-resolution rules

Project discovery should use the authoritative shared container services, not hardcoded scans of per-app directories.

## Current Migration Truths

As of July 29, 2026:

- `shared/AssetSystem` is the main suite storage/container/VFS path.
- `shared/UI` owns the real shared suite shell behavior for migrated apps.
- Creation Station can link a project to a suite container and route asset operations through the shared container path while still supporting older folder-only projects.
- Creation Engine has been cut over away from its local VFS target and now rides on the shared suite VFS path through a compatibility wrapper.
- Creation Live is still early and not fully wired to the shared asset/container path yet.
- Movie still has some cleanup left around the last duplicated LLVM helper path.

## How To Add A New App

1. Start from `templates/SuiteProjectShell/`.
2. Link to shared targets instead of copying infrastructure:
   - `creation_suite_asset_system`
   - `creation_suite_services`
   - `creation_suite_ui`
   - `creation_suite_interop`
3. Give the app its own branding, domain workflows, and domain policy.
4. Do not create app-private versions of suite settings, asset systems, top bars, or container logic.

## How To Touch Project Storage Correctly

For a new suite-owned workflow:

1. Load suite settings through `creation::suite::SuiteSettingsStore`.
2. Open or create a project through `creation::assets::ProjectWorkspaceService`.
3. Import/read/materialize/reconcile assets through the shared asset/container APIs.
4. Commit through the shared session/workspace path.

If you are touching an older app that still has folder-era code:

1. Prefer adding a compatibility bridge to the shared path.
2. Preserve fallback behavior for old projects when necessary.
3. Avoid rewriting the whole app storage model in one pass unless explicitly requested.

## What Not To Do

- Do not add a second VFS implementation in an app.
- Do not add a second suite settings store in an app.
- Do not make project discovery by scanning arbitrary folders when `ProjectContainerService` can answer it.
- Do not make the header responsible for transport.
- Do not copy shared source files into a new app just because it is faster in the moment.

## Practical Decision Rule

Ask this before writing code:

"Is this behavior suite-owned or app-owned?"

If the answer is any of these, it is suite-owned and belongs in `shared/`:

- containers
- assets
- materialization
- suite settings
- shared shell/header actions
- AI account/provider infrastructure
- logging
- cross-app discovery

If the answer is branding, domain workflow, app-specific tools, or app-specific policy, it belongs in the app.
