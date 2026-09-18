# Creation Suite Infrastructure Guide For LLMs

Date: July 29, 2026

This document explains how an LLM should work inside the Creation Suite codebase without reintroducing app-local infrastructure drift.

This doc is subordinate to the top-level `AGENTS.md`, specifically its Development Environment Rule: there is no backward compatibility requirement anywhere in this codebase, this is new development, and legacy filesystem-first behavior is not to be preserved. If anything below reads as sanctioning a compatibility wrapper, dual-storage support, or a fallback path for old data, that wording is wrong and should be fixed, not followed — treat old folder-era project structures as disposable, full stop.

## Bottom Line

If you are adding or changing shared behavior, prefer `shared/` first.

Do not build new app-local persistence, VFS, suite settings, account/provider settings, shared shell behavior, or project-container logic inside a specific app. No exceptions — not for a "temporary" compatibility wrapper, not for a migration bridge, not for "just in case." If old app-local code exists doing this, cut it over to the shared implementation and delete the old code in the same pass; do not wrap it.

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

Do not add new app-local copies of these behaviors.

### Shared Interop

Primary location:

- `shared/Interop/`

Use this for:

- project discovery
- cross-app project/asset lookup
- future compatibility and reference-resolution rules

Project discovery should use the authoritative shared container services, not hardcoded scans of per-app directories.

## Current Migration Truths

As of August 3, 2026 — stated as known debt to eliminate, not as sanctioned dual-mode behavior:

- `shared/AssetSystem` is the main suite storage/container/VFS path.
- `shared/UI` owns the real shared suite shell behavior for migrated apps.
- Creation Station still has old folder-only-project code paths (`.patina.json` extension handling in `ProjectStorage.cpp`, a legacy per-app AI-settings migration in `MainComponent.cpp`) that have not yet been removed. These are bugs to fix, not a supported "old projects still work" mode — do not extend or imitate this pattern elsewhere.
- Creation Engine's shared-VFS integration was cut over directly onto `creation::assets::VirtualFileSystem`; the old `ce::assets` compatibility-wrapper header has been deleted.
- Creation Live is still early and not fully wired to the shared asset/container path yet.
- Movie has already been fully cut over (its old `.creationmovie` flat-file format was deleted outright, not wrapped) — this is the pattern to follow for the other apps, not Station's or Engine's current state.

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

1. Cut it over to the shared path directly. Do not add a compatibility bridge.
2. Do not preserve fallback behavior for old projects — treat old folder-era data as disposable test data, per `AGENTS.md`.
3. A full rewrite of the app's storage model in one pass is the expected outcome when the task touches that model, not something to avoid — the exception is scoping down to a smaller, explicitly-requested slice of the work, not leaving old and new paths coexisting.

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
