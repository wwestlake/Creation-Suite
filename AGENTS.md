# Agent Instructions

## Scope

These instructions apply to the umbrella `Creation Suite` repository.

App-specific repositories under `apps/` may also have their own `AGENTS.md` files. When working inside an app repo, follow both:

- this top-level suite policy
- the app-local policy

If they conflict, stop and ask the user which rule should govern the task.

## Development Environment Rule

This workspace is a development/evaluation environment by default.

- Do not preserve legacy filesystem-first project behavior unless the user explicitly asks for backward compatibility.
- Do not keep dual storage authority alive "just in case" if the active task is a suite VFS/container cutover.
- If a suite-managed container/VFS path and a legacy folder/project-file path both exist, prefer making the suite path authoritative and removing the legacy runtime path rather than synchronizing both.
- Treat old folder-era project structures as disposable test data in this environment unless the user explicitly says real assets or historical projects must be preserved.
- Do not reintroduce filesystem-first fallbacks, migration glue, or dual-write logic as a default safety measure when the user's goal is to stop using the filesystem model.

## Path Rule

- No hard-coded filesystem paths in code. Ever.
- Do not hard-code absolute Windows paths, developer-machine-specific roots, fixed user directories, or baked-in local workspace locations into runtime code.
- Resolve locations through the suite settings/bootstrap system, shared storage helpers, configuration, or explicit user choice where appropriate.
- If code still depends on a specific filesystem layout, treat that as technical debt to remove rather than a pattern to extend.

## UI Context Menu Rule

Context menus in Creation Suite follow two different anchoring models, and they are not interchangeable:

- Left-click button menus are command-source menus. They belong to the button the user activated, so they should open anchored to that button/component.
- Right-click menus are spatial context menus. They belong to the exact place and object under the pointer, so they must open at the pointer location, not at some unrelated toolbar button or arbitrary component edge.

Implementation rule:

- For button-triggered menus, use component anchoring (`withTargetComponent(...)`).
- For right-click context menus on canvases, timelines, editors, nodes, ports, clips, connections, and similar work surfaces, convert the click point to a screen-area anchor and use pointer-position anchoring (`withTargetScreenArea(...)`).
- The menu contents must also depend on what was under the cursor: empty surface, node, port, connection, clip, track cell, scope, generator, etc. Position and content are both context-sensitive.

Do not collapse these into one generic "popup menu" pattern. If the user right-clicked a location in the workspace, mouse location is authoritative.

This rule is mandatory, not stylistic. If a right-click menu is anchored to an unrelated toolbar button, parent panel edge, or generic fallback location, that implementation is wrong and should be treated as a bug.

## Shared Ownership Rule

`shared/` is the highest-risk collision area in this workspace.

- Only one writable owner should modify `shared/` at a time.
- If another agent is doing suite-platform work, do not also edit `shared/` in the same checkout.
- Do not treat another agent's untracked files as authoritative without explicit confirmation.

## Multi-Agent Rule

Each LLM has its own fully independent `git clone` of the Creation Suite umbrella repo, with its own independently-cloned submodules underneath (`apps/CreationEngine`, `apps/CreationLive`, `apps/CreationMovie`, `apps/CreationStation`) — not a git worktree, not a shared checkout. Each one has its own separate `.git` object database, so nothing done in one is visible in another until it is pushed to and fetched from GitHub.

- **Claude Workspace**: `D:\CreationSuite-Workspaces\CreationSuite-Claude` (Branch prefix: `claude/...`)
- **Codex Workspace**: `D:\CreationSuite-Workspaces\CreationSuite-Codex` (Branch prefix: `codex/...`)
- **Gemini Workspace**: `D:\CreationSuite-Workspaces\CreationSuite-Gemini` (Branch prefix: `gemini/...`)

**`D:\000 Creation Suite` is the user's own personal clone. No LLM may read, explore, build in, or otherwise touch it, for any reason** — with exactly one narrow, explicit exception: reading (never writing to) `D:\000 Creation Suite\apps\CreationEngine\vcpkg_installed` as the source for the one-time LLVM bootstrap copy described below. Nothing else in that directory is fair game. Do not extend this exception to any other file or purpose without the user saying so again, in that conversation.

### Getting LLVM working in your own workspace

Creation Engine needs a real, already-built LLVM install at `apps/CreationEngine/vcpkg_installed/x64-windows/` to compile `Language/`. Building it from source takes hours — never do that without an explicit, in-the-moment yes from the user (see the LLVM / vcpkg Build Rule below). Instead:

1. Copy `D:\000 Creation Suite\apps\CreationEngine\vcpkg_installed` (a real, already-extracted install — ~11GB, contains `LLVMCore.lib` etc.) into your own workspace's `apps\CreationEngine\vcpkg_installed`. Plain file copy, no vcpkg command, no build — this is the one exception noted above.
2. If that source ever goes stale or missing, the fallback is `vcpkg install` with `VCPKG_DEFAULT_BINARY_CACHE` pointed at `D:\vcpkg-cache` (see Engine's own README, per the Documentation Index below) — still needs an explicit yes first, since it can silently fall back to a real from-source build if the cache doesn't match.

Do not use the same writable checkout as another agent. Never assume that untracked files, half-finished edits, or local build outputs in another agent's checkout are safe to build on — each agent's checkout is independent and may be ahead, behind, or diverged from any other at any time. To get current shared code, `git pull`/`git fetch` your own clone against `origin` — never by reading another agent's directory.

Reading reference material from another agent's workspace (scripts, templates, docs) to use as a pattern or run as a tool is fine. Writing, committing, or building directly against files inside another agent's workspace is not — copy what you need into your own workspace first.

### Personal Development Branch Rule

Each agent maintains exactly one standing, personally-named development branch per repo — `claude/development`, `codex/development`, `gemini/development` — in every repo that agent touches (the umbrella repo and any app repo). This branch is that agent's own integration point for ALL of that agent's work in that repo, across sessions:

- Do not create a fresh one-off branch off `master`/`main` for every task and leave it stranded there. Branch task work off your own `<agent>/development` (creating it first if it doesn't exist yet in a given repo), and merge each finished, verified piece of work back into it as you go.
- **Merge and push per completed task, not batched at the end.** The moment one identified task is verified (builds, tests pass), merge it into `<agent>/development` and push that branch to `origin` immediately — in the same turn. Do not accumulate several finished tasks locally and merge them all together later; each completed task gets its own merge-and-push cycle.
- `<agent>/development` should always build and pass its test/smoke checks. Do not merge broken or unverified work into it.
- Only once `<agent>/development` builds and passes testing does it become eligible for a PR to `master`/`main` on the server — opening that PR still requires the same explicit push/merge authorization as any other push to a shared branch (see Push Authorization Rule below); reaching "buildable and tested" is a precondition for asking, not authorization by itself.
- Short-lived task branches are still fine for isolating one piece of in-progress work, but they should merge into `<agent>/development` when done, not sit alongside it indefinitely as siblings nobody reconciles.
- Before starting new work in any repo, check whether your own `<agent>/development` branch already exists there and what's on it — real, completed work can be sitting there from an earlier session.

## Secrets Directory Rule

The `Secrets/` directory contains critical credentials and private keys.
- `Secrets/` MUST remain in `.gitignore` at all times.
- **NEVER** stage, commit, or push any file inside `Secrets/` to any remote repository.

### Branch Ownership Check

Before committing anything, check `git rev-parse --abbrev-ref HEAD`. Branch names in this workspace follow an owner/topic convention (`codex/...`, `claude/...`, `gemini/...`, etc.) — if the current branch's prefix names a different agent, **that branch is not yours to commit on**, regardless of who happens to have it checked out right now.

- If the checked-out branch belongs to another agent, create and switch to your own branch (matching your own name prefix) before making any commit — do not commit to someone else's named branch just because it was already checked out.
- This applies even for small, low-risk changes (a doc fix, a wiki edit) — branch ownership isn't scaled by risk, same as the Push Authorization Rule below.
- This rule exists because it was already violated: work happened for an entire session directly on `codex/suite-save-20260729` — a Codex-owned branch by naming convention — without it ever being handed off.

## Integration Rule

Only one actor should act as the integrator at a time.

The integrator is the only one who should:

- merge branches
- resolve conflicts
- update umbrella-repo submodule pointers
- declare work fully landed

If you are not explicitly the integrator, stop at a clean branch and report:

- repo
- branch
- files touched
- what was verified
- what remains risky or blocked

### Push Authorization Rule

**Pushing your own dedicated branch to `origin` is routine, not gated.** A local-only branch is volatile — it exists on one machine, in one clone, invisible to every other agent and to the user until it's on GitHub. After every commit on a branch you created for your own work (`claude/...`), push it to `origin` in the same turn, no permission needed. This is git hygiene, the same category as creating the branch in the first place — do it, don't ask.

**What still requires an explicit, in-the-moment instruction from the user, in that exact conversation:**

- Pushing to `main`/`master`, or to any branch you don't own (another agent's `codex/...`/`gemini/...` branch, a shared integration branch).
- Merging a branch into `main`/`master`, opening a PR that requests merge, or otherwise advancing work past "parked on my own branch, visible on GitHub."
- Force-pushing anything, ever — including your own branch, if it means discarding commits that exist elsewhere.

None of the following count as authorization for the gated actions above:

- a request that would merely be *satisfied* by something existing on the remote (e.g. "give me a link to that file" does not mean "merge it")
- a prior approval to merge/force-push something else, earlier or elsewhere
- the fact that a task feels finished or ready

If completing a request seems to require one of the gated actions and you are not the designated integrator for that repo, **stop and say so explicitly** before doing it: name the repo/branch, say what's needed, and ask. Do not act first and explain afterward.

This rule was rewritten after repeated real-world friction: agents were treating "never push" as blanket policy and leaving work stranded in local-only branches indefinitely — the opposite of the original intent, which was only ever to gate *merging into main* and *force-pushing*. The original violation this rule was built to prevent (a request for "a link I can paste into a ticket" read as implicit permission to commit and push, which also carried unrelated staged submodule-pointer changes along with it) is still fully covered — that was a push to a shared/integration context, not a routine push of your own branch. See Commit Scope Rule below.

## Commit Scope Rule

Before running `git commit` in ANY repo in this workspace — the umbrella repo, an app repo, or a wiki repo — run `git status --short` immediately before staging and stage only the specific files intended for that commit.

- Never assume the index is clean. Content staged earlier by another agent (or by you, earlier in a session) can silently ride along into an unrelated commit.
- Never use a broad `git add -A` / `git add .`.
- **Wiki commits contain wiki content only.** A commit made inside `wiki/` or any `apps/*/wiki/` checkout must never include changes from the paired code repo, and vice versa.
- If `git status` shows anything staged that you didn't just add yourself and can't explain, stop and ask before committing — don't push it along "because it was already there."
- This rule exists because it has already been violated once: a single new spec file committed via `git add <file>` (skipping a pre-commit `git status` check) swept in four unrelated submodule-pointer bumps that were already staged, and the resulting commit was pushed before anyone noticed. Submodule-pointer updates are the integrator's job per the Integration Rule above — they should never land as a side effect of an unrelated commit.

## Build Rule

Avoid sharing scratch build folders across multiple active agents.

If an app repo has a fixed build-directory rule in its own `AGENTS.md`, respect it. If that makes safe parallel writable work impossible in that checkout, use a separate worktree or do not parallelize that repo.

### Build Configuration Rule

Use `Debug` builds by default for normal development, testing, and troubleshooting.

- `Debug` is the standard day-to-day development build.
- `Release` is only for intentional release work, such as a tagged version release or explicit release-build verification requested by the user.
- Do not assume `Release` just because a build is meant to be runnable.
- When reporting a build result, name the configuration you actually built.

### Shared Bin Directory Rule

Every agent maintains its own workspace-level shared bin directories, one per build configuration, at `D:\CreationSuite-Workspaces\<agent>-debug-bin\` and `D:\CreationSuite-Workspaces\<agent>-release-bin\` (e.g. Claude's are `claude-debug-bin`/`claude-release-bin`). These give a stable, no-need-to-hunt-for-it path to the latest built executable of every app in the suite, regardless of which app's build tree it actually lives in.

For every real app executable target in your own repo (not test/smoke binaries), add a `POST_BUILD` custom command to that target's `CMakeLists.txt` that copies the built binary into your agent's bin directory for the configuration just built:

```cmake
add_custom_command(TARGET <YourTargetName> POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "D:/CreationSuite-Workspaces/<agent>-$<LOWER_CASE:$<CONFIG>>-bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:<YourTargetName>>" "D:/CreationSuite-Workspaces/<agent>-$<LOWER_CASE:$<CONFIG>>-bin/"
    COMMENT "Copying <YourTargetName> to the shared <agent> bin directory"
)
```

Replace `<agent>` with your own name (`claude`, `codex`, `gemini`, ...) and `<YourTargetName>` with the real target. `$<LOWER_CASE:$<CONFIG>>` resolves to `debug`/`release` automatically, so one command handles both configurations.

This is a plain file copy at build time -- it does not require OS symlink privileges (creating a real Windows symlink needs Administrator or a freshly-logged-in session with Developer Mode already active, which made it the wrong tool for this). Do not attempt to symlink instead; copy.

## Merge Rule

Do not merge just because code exists locally.

Before reporting something as complete or integrated, verify:

- it is committed in the correct repo
- it is on the correct branch
- any required submodule pointer updates are done
- the relevant build or smoke checks were actually run when possible

## GitHub Project Board Rule

Always use GitHub Issues and the official GitHub Project Board (**Creation Suite Road Map** - Project #19) to track feature work, tasks, and completion status.

- **Shorthand Definition**: Whenever the user says "create a task", "add tasks", "track tasks", or similar, it ALWAYS means: *"Create real GitHub Issues on the repository for the active app/component (`gh issue create`) and add them directly to the **Creation Suite Road Map** project board (`gh project item-add 19 --owner wwestlake --url <issue-url>`)"*.
- **Status Sync Rule**: Every time an agent completes a task, fixes an issue, or changes work status, the agent MUST immediately update the corresponding GitHub Issue and Project Board item on **Creation Suite Road Map** (Project #19), adding completion comments (`gh issue comment`) and explicitly setting the project board item Status field to **Ready for Testing** (`gh project item-edit --id <item-id> --project-id PVT_kwHOADBc_84Bet07 --field-id PVTSSF_lAHOADBc_84Bet07zhZF9co --single-select-option-id a822205a`).
- Do not use temporary local `.md` task files as the primary task tracker.

## LLVM / vcpkg Build Rule

Before running any command that touches `vcpkg`, `Language/`, or CEL/LLVM build setup in Creation Engine, read `apps/CreationEngine/README.md`'s **"Scripting language build (LLVM via vcpkg)"** section first, in full, every time — see the Documentation Index below. LLVM must never be rebuilt from source without an explicit, in-the-moment yes from the user in that exact conversation, no matter how the task is framed or how urgent it seems — same standard as the Push Authorization Rule above. Starting a build in the background and treating a same-turn status update as having asked does not satisfy this.

## Documentation Index

This file is a MAP, not a manual — it tells you which document governs a given topic; it does not restate that document's content. Before starting work in an area below, open the linked file(s) and read them, live, every session — do not rely on a memory of having read them before, and do not let anything here substitute for actually opening the file.

**Suite-wide:**
- Multi-agent concurrency/handoff process → [docs/MULTI_AGENT_WORKFLOW.md](docs/MULTI_AGENT_WORKFLOW.md)
- Suite platform architecture → [docs/SUITE_PLATFORM_ARCHITECTURE.md](docs/SUITE_PLATFORM_ARCHITECTURE.md), [docs/architecture/Suite-Architecture.md](docs/architecture/Suite-Architecture.md)
- Suite authored-control / trigger boundary → [docs/architecture/Suite-Control-Boundary-Plan.md](docs/architecture/Suite-Control-Boundary-Plan.md)
- Shared project model (one project = one VFS, any app opens it, no import/export between suite apps) → [docs/architecture/Suite-Shared-Project-Model.md](docs/architecture/Suite-Shared-Project-Model.md) — read before any cross-app project/asset work; corrects the "cross-app import" framing that used to appear elsewhere in this index.
- Real-time collaboration (DCC-over-the-internet, LagDaemon.com broker/P2P) → [docs/architecture/Suite-Realtime-Collaboration-Plan.md](docs/architecture/Suite-Realtime-Collaboration-Plan.md) — quick-capture spec, not yet filed as issues; larger/harder to scope than most docs here, review before breaking into milestones.
- Suite Agent Loop (Virtual Engineer orchestration: perceive/reason/act/observe/verify, built on SuiteContextEngine + set_state) → [docs/architecture/Suite-Agent-Loop-Plan.md](docs/architecture/Suite-Agent-Loop-Plan.md) — quick-capture spec, not yet filed as issues.
- Shared-library extraction plan/status → [docs/SHARED_EXTRACTION_PLAN.md](docs/SHARED_EXTRACTION_PLAN.md), [docs/SHARED_INFRASTRUCTURE_AUDIT_2026-07-28.md](docs/SHARED_INFRASTRUCTURE_AUDIT_2026-07-28.md)
- Asset/VFS storage standard → [docs/standards/Suite-Asset-VFS-Standard.md](docs/standards/Suite-Asset-VFS-Standard.md)
- Project template scaffolding → [docs/Project-Template-Generator.md](docs/Project-Template-Generator.md), [docs/Suite-Project-Template-LLM-Instructions.md](docs/Suite-Project-Template-LLM-Instructions.md)
- Cross-app migration history/plans → [docs/migration/Migration-Plan.md](docs/migration/Migration-Plan.md), [docs/migration/Station-Engine-First-Moves.md](docs/migration/Station-Engine-First-Moves.md)
- LLM/agent infrastructure guide → [docs/LLM-INFRASTRUCTURE-GUIDE.md](docs/LLM-INFRASTRUCTURE-GUIDE.md)
- Each `shared/*` library has its own `README.md` (`shared/README.md` for the overview, then `shared/AssetSystem`, `shared/CEL`, `shared/CMake`, `shared/Interop`, `shared/NodeSystem`, `shared/Services`, `shared/UI`) — read the specific one before changing that library.
- CEL language design (types, memory model, functional-paradigm features, the real-time-safe execution profile, modules) → [shared/CEL/docs/CEL_V2_LANGUAGE_SPEC.md](shared/CEL/docs/CEL_V2_LANGUAGE_SPEC.md) — read before touching CEL's grammar, sema, or intrinsic surface; tracked as GitHub issues #49–#59 on Creation-Suite.
- Hermes (CEL package manager/distribution, LagDaemon.com store) → [shared/CEL/docs/HERMES_PACKAGE_MANAGER_SPEC.md](shared/CEL/docs/HERMES_PACKAGE_MANAGER_SPEC.md) — quick-capture spec, not yet filed as issues; read before scoping any package-management work.

**Per app** (`apps/CreationEngine`, `apps/CreationMovie`, `apps/CreationStation`, `apps/CreationLive`): each has its own `AGENTS.md` (app-local policy, binding alongside this file per Scope above), `README.md` (build/setup instructions — e.g. Engine's LLVM/vcpkg section), and `docs/` folder. Always check for and read an app's own `README.md`/`AGENTS.md`/`docs/` before working in it; do not assume this index is exhaustive of what's there. Known app-specific docs as of this writing:
- Creation Engine: [docs/CAPABILITIES.md](apps/CreationEngine/docs/CAPABILITIES.md) (scope/non-goals), [docs/GS_SCRIPTING_PLAN.md](apps/CreationEngine/docs/GS_SCRIPTING_PLAN.md) (CEL milestone plan), [docs/SCRIPTING_ABI.md](apps/CreationEngine/docs/SCRIPTING_ABI.md) (host ABI spec), [docs/CROSS_APP_LANGUAGE_DOMAINS.md](apps/CreationEngine/docs/CROSS_APP_LANGUAGE_DOMAINS.md) (intrinsic domain gating), [docs/CROSS_APP_ASSET_INTEROP.md](apps/CreationEngine/docs/CROSS_APP_ASSET_INTEROP.md)
- Creation Movie: [docs/CAPABILITIES.md](apps/CreationMovie/docs/CAPABILITIES.md), [docs/LANGUAGE_ROLLOUT.md](apps/CreationMovie/docs/LANGUAGE_ROLLOUT.md)
- Creation Station: [docs/Creation-Shared-Language-Rollout.md](apps/CreationStation/docs/Creation-Shared-Language-Rollout.md), [docs/Creation-Suite-AssetSystem-Standard.md](apps/CreationStation/docs/Creation-Suite-AssetSystem-Standard.md), [docs/Creation-Suite-Interop-Spec.md](apps/CreationStation/docs/Creation-Suite-Interop-Spec.md), [docs/Studio-Grade-Audio-Routing-Checklist.md](apps/CreationStation/docs/Studio-Grade-Audio-Routing-Checklist.md), [docs/Creation-Station-0.5.0-Beta-Checklist.md](apps/CreationStation/docs/Creation-Station-0.5.0-Beta-Checklist.md), [docs/STATION_CONTROL_REGISTRY_SEED.md](apps/CreationStation/docs/STATION_CONTROL_REGISTRY_SEED.md) — concrete set_state/get_state entries for the CEL control registry, seeded from a real codebase investigation; [docs/Signal-Lab-Character-Macros-Spec.md](apps/CreationStation/docs/Signal-Lab-Character-Macros-Spec.md) — Signal Lab's high-level sound-character macro definitions and guardrails; [docs/Signal-Lab-Node-Graph-Spec.md](apps/CreationStation/docs/Signal-Lab-Node-Graph-Spec.md) — living spec for Signal Lab's node-graph system (port anatomy, connections, runtime model, Sources/Sinks); read before touching node/port/connection code, and keep it updated as requirements are given or changed

**Task/status tracking:** GitHub Issues + the **Creation Suite Road Map** project board (Project #19) — see the GitHub Project Board Rule above, not a doc file.

**Keeping this index honest:** if you find a document that governs a topic covered by a task you're doing, and it isn't listed here, add it here as part of that task — this index is only useful if it stays current. Conversely, if a linked file no longer exists or has been superseded, fix or remove the entry rather than leaving a dead pointer.
