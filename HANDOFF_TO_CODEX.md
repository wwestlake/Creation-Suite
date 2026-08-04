# Handoff to Codex — Zero-Backward-Compatibility Cleanup + Suite VFS Service

Date: 2026-08-04
From: Claude (branch `claude/zero-backward-compat-cleanup`, now merged to `master`)

## What this covers

Two things landed together this pass, in the umbrella repo plus `apps/CreationEngine` and `apps/CreationStation`:

1. A suite-wide cleanup enforcing the "no backward compatibility, no legacy filesystem-first behavior" rule already stated in `AGENTS.md`.
2. A new piece of core infrastructure: a suite-owned background VFS service.

## 1. Backward-compat cleanup

- **`docs/LLM-INFRASTRUCTURE-GUIDE.md`** was actively instructing agents to build "temporary compatibility wrappers" and "preserve fallback behavior for old projects" — directly contradicting `AGENTS.md`'s Development Environment Rule. Stripped out. If you're reading agent-facing docs in this repo for guidance and see language sanctioning a compat wrapper or fallback path, that's a bug in the doc, not something to follow.
- **CreationEngine**: deleted `shared/AssetSystem/include/assets/VirtualFileSystem.h` (an old `ce::assets` namespace compat shim wrapping the real `creation::assets::VirtualFileSystem`). Five files cut over to the real type directly (`AssetImporter.h`, `AssetCatalog.{h,cpp}`, `ViewportComponent.{h,cpp}`, `ShaderComposer.{h,cpp}`, `GltfLoader.{h,cpp}`). Build-verified.
- **CreationStation**: removed a legacy AI-provider-settings migration path (`loadSuiteAiProviderSettings`'s `migrateLegacyIfNeeded` branch, and `loadAppSettings`'s legacy `settings.xml` AI-field parsing) and the `.patina.json` legacy extension alias in `ProjectStorage.cpp`. Patina (Station's old bespoke DSL) was already removed in an earlier commit (`58917b1`); this pass cleaned up the remaining docs/comments that still described it as live.
- **`shared/CMake/CreationSharedLLVM.cmake`** / **`CelGrammar.cmake`**: removed hardcoded developer-machine paths (`D:/000 Creation Engine/...`, `D:/tools/winflexbison`), replaced with explicit `-DCE_LLVM_VCPKG_DIR=` / `-DCE_WINFLEXBISON_DIR=` overrides or normal `find_program` PATH resolution.

**Known still-open** (found by a suite-wide filesystem-write audit, not fixed this pass — see `docs/architecture/Suite-Shared-Project-Model.md` for the full list): `suite-legal-settings.json`, `suite-activity-log.json`, `suite-ai-health.json`, `suite-ai-diagnostics.json`, and the per-process heartbeat files in `SuiteProcessRegistry` all still write as loose files next to the real bootstrap JSON. Same fix (route through the new VFS service, below) applies whenever someone picks this up.

## 2. New: the suite-owned VFS service

`docs/architecture/Suite-Shared-Project-Model.md` had an explicitly open question: how do multiple app processes coordinate writes to shared suite-level data without a real multi-writer race. This pass answers it and builds it.

**Design**: a new background process, `services/VfsService`, is the *only* process that ever opens a reserved "suite root project" container directly (`SuiteAppDomain::suite`, via the existing `ProjectSession`/`ProjectContainerService` API — no new storage mechanism, no special-casing). It exposes that project over a localhost-only HTTP + WebSocket API:

- `GET /health`
- `GET/PUT/DELETE /suite/entry?path=<name>` (entries live under `suite/` inside the container)
- `GET /suite/entries`
- WebSocket broadcasts `{"event":"entryChanged","path":"..."}` after every committed write

It registers itself in `SuiteProcessRegistry` (new `httpPort` field on `SuiteProcessRecord`) so any app can discover it via `SuiteVfsServiceClient` (`shared/Services`) — which also launches the service if it isn't already running. The service self-exits once no other suite app has been alive for a grace period (20s).

**Already migrated onto it**: `SuiteAiSettingsStore` (AI provider accounts + API keys) and `SuiteDesktopAuthSession` (auth token/session — Station's own duplicate `DesktopAuthSession` class was deleted outright in favor of the shared one). Neither writes a loose file anymore.

**Verified end-to-end**: service builds, creates the root project on first run, all HTTP endpoints round-trip correctly, shuts down clean (exit 0, no crash). One real bug worth knowing about if you touch `services/VfsService/Source/Main.cpp`: the root project session and `SuiteProcessRegistration` must be declared as **locals in `main()`, after `juce::ScopedJuceInitialiser_GUI`** — an earlier version had the session as a file-scope global, which outlived JUCE's teardown and crashed on exit (classic static-destruction-order bug). Don't move it back to a global.

**Explicitly not done yet**: regular per-app project open/save (all 6 apps' existing `ProjectSession`-direct flows) still goes direct to the container, not through this service. That's a much larger, separate migration — this pass proved the mechanism on the smaller, security-critical surface first.

## Dependencies added

`services/VfsService/vcpkg.json` adds `cpp-httplib` and `ixwebsocket` (both trimmed to `default-features: false` / core-only — no TLS, no compression — since this is localhost-only). First configure will trigger a vcpkg build of these (~10 min uncached, seconds once cached).

## One thing NOT included in this push

There's other in-progress, **uncommitted** work sitting in local checkouts on this machine (an unrelated "Suite Asset Manager" panel feature, touching `shared/UI` and each app's `MainComponent`) that predates this session and was never part of this cleanup. Since it was never committed, it isn't part of what got merged/pushed here — it may still exist uncommitted in whichever checkout was working on it. Don't be surprised if you don't see it; it wasn't lost, it just isn't here.
