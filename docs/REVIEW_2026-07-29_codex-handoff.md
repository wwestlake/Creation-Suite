# Review Report — Issues Found, 2026-07-29 (for Codex)

Scope: findings from a code-vs-wiki audit pass across Station/Engine/Movie/Live this afternoon. This is issues only — general status is already current in the suite wiki (`wiki/Roadmap.md`, `wiki/System-Architecture.md`, per-app pages). Verified directly against files on disk at time of writing; re-check before acting if time has passed given how fast this repo moves.

## 1. Blocking — Creation Engine build environment is broken

**File:** `apps/CreationEngine/build/CMakeCache.txt`
**Problem:** the configured cache still resolves `EnTT_SOURCE_DIR` and `LLVM_DIR` to fallback paths under `D:/000 Creation Engine/...` — that directory does not exist on this machine. `apps/CreationEngine/third_party/entt` (submodule) is unpopulated (empty dir).

**Consequence:** every existing built executable in `apps/CreationEngine/build/` (`CreationEngineServer.exe`, `frustc.exe`, `Creation Engine.exe`) predates both the 2026-07-29 shared-header-bar commit and the latest CMake reconfigure. **Nothing has successfully rebuilt since the shared-AssetSystem VFS cutover landed.** The cutover itself looks correct in source (see §4), but it is unverified by any actual build.

**Action needed:** either populate `third_party/entt` (submodule init/update) and point `LLVM_DIR` at a real local LLVM/vcpkg install, or delete `build/` and do a clean reconfigure with correct paths, then do one full build to confirm the AssetSystem cutover actually compiles.

## 2. Needs reconciliation — Creation Station per-track FX chain status

**File:** `apps/CreationStation/docs/Creation-Station-0.5.0-Beta-Checklist.md:61`
**Problem:** `- [ ] Support track FX chains.` is still unchecked — only "Support single track insert FX" and "Support master insert FX" are checked. But commit history includes `4661c93` ("Make per-track FX chains real and visible in the Tracker") and `b6d6fe2` ("Redesign Track FX Stack window as a Reaper-style two-pane browser"), which read as if the feature landed.

**Action needed:** whoever owns Station's Tracker/FX work should verify actual current behavior in the app and either check the box (if it's really done) or correct the commit-message framing (if it's still single-insert-FX only). Right now the checklist and commit history contradict each other, so status is unverifiable from docs alone.

## 3. Needs owner confirmation — uncommitted WIP in Creation Station

**File:** `apps/CreationStation/Source/Project/ProjectStorage.cpp`
**Problem:** 207 lines of uncommitted, unstaged changes sitting in the working tree, adding `loadLinkedSuiteProject(...)` and pulling in `creation/assets/ProjectSession.h` / `ProjectWorkspaceService.h` — wiring `.csp` project load/save into the shared suite container/VFS path.

**Action needed:** per the multi-agent workflow policy (`docs/MULTI_AGENT_WORKFLOW.md`), untracked/uncommitted content in a shared checkout isn't authoritative until its owner confirms it. If this is your in-progress work, commit it (or stash/branch it) so it doesn't get silently overwritten or misread as "not started" by another agent. If it isn't yours, don't build on it yet — confirm ownership first.

## 4. Process decision needed — CreationTexture / CreationModeler have no repo identity

**Files:** `apps/CreationTexture/`, `apps/CreationModeler/`
**Problem:** both exist as real local scaffolds (generated via `scripts/New-SuiteProject.ps1`) with their own local `.git` init, but neither is registered in `.gitmodules`, neither has a GitHub remote, and multiple agents are already writing into both (domain-code work for Texture is anticipated per `docs/architecture/Creation-Texture-Functional-Spec.md`). This is exactly the "shared checkout with no clear owner" situation the workflow doc warns about, except one level up — there isn't even a shared remote to coordinate through yet.

**Action needed:** a decision from the human lead on whether/when to stand up real GitHub repos and register them as submodules, before parallel domain-code work ramps up on either app. Not something to resolve unilaterally.

## Already fixed this pass (FYI, no action needed)

- **`apps/CreationEngine/wiki/Architecture.md`** previously described `AssetSystem/` as a local `asset_system` build target — factually wrong post-cutover (the target no longer exists; Engine now links `creation_suite_asset_system` from `shared/`). Corrected in place, with a note that the cutover is unverified by a build (see §1).
- **`wiki/Roadmap.md`** "Known Duplication to Remove" still listed Station's suite-settings storage/panel as pending removal — those files were already deleted in commit `9d63aba` (2026-07-28). Corrected.
- Confirmed **not** issues, despite earlier notes suggesting otherwise: Movie's LLVM CMake helper now correctly includes the shared `shared/CMake/CreationSharedLLVM.cmake` (no longer a local duplicate); Station's `AiProviderSettings.h` is now a clean 5-line type alias to `creation::services::SuiteAiResolvedRuntimeSettings`, not a duplicate implementation.
