# Agent Instructions

## Current repository authority and PR workflow (October 25, 2026)

The Executive Director owns all repositories. Codex is the designated working
agent for the entire Creation Suite structure, including applications, shared
libraries and submodules, until the Executive Director reassigns it. Do not
request ownership negotiations or LLM handoffs under this assignment.

All owned repositories use `master` as their primary branch. Start a task from
clean, integrated `master` on a fresh task-specific branch. Preserve unfinished
work. Complete implementation and appropriate tests, commit and publish a PR to
`master` for the Executive Director's review/merge. Required reviews/checks must
not be bypassed. After an actual merge, remove the completed branch, integrate
dependent gitlink updates through PRs, and verify clean repository states.
An open PR is ready for review, not integrated work. This supersedes conflicting
ownership-transfer and automatic direct-merge instructions below.

## Tool Paths

- **Node.js (node / npm / npx)**: `D:\tools\node22\node-v22.20.0-win-x64\node.exe` and `D:\tools\node22\node-v22.20.0-win-x64\npm.cmd`. Node is installed but not on the PATH. Always use absolute paths or set up the environment block when running web builds.
## Scope

These instructions apply to the umbrella `Creation Suite` repository.

App-specific repositories under `apps/` may also have their own `AGENTS.md` files. When working inside an app repo, follow both:

- this top-level suite policy
- the app-local policy

Current user instructions take precedence. The October 17 shared-workspace and task-completion policy below supersedes older per-agent workspace/branch/build-drop rules. Ask only about a remaining conflict that the user has not already resolved.

## USER CODE AUTHORSHIP RULE — READ THIS FIRST

**THE USER DOES NOT WRITE CODE IN THIS PROJECT. THE USER IS NOT A CODE CONTRIBUTOR.**

**ANY CODE, DOCUMENTATION, CONFIGURATION, OR OTHER UNCOMMITTED WORK FOUND IN THIS
WORKSPACE WAS CREATED BY AN AI AGENT, UNLESS THE USER EXPLICITLY SAYS OTHERWISE.**

**UNCOMMITTED WORK MUST BE TREATED AS AGENT WORK THAT WAS LEFT UNCOMMITTED — NEVER
AS USER-AUTHORED CODE. PRESERVE IT, IDENTIFY IT, AND FINISH OR COMMIT IT SAFELY.**

The user provides requirements, reviews results, and approves merges. Agents own the
implementation and the responsibility to commit their work before reporting a task
complete.

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

## LiteSemRAG Cards and the Virtual Engineer

The suite's AI is one shared component, the Virtual Engineer (`shared/VirtualEngineer`, README there): every app asks the AI through it, with the suite's AI account and key (`ai-settings.json`, entered once in the suite's settings - no app asks for or stores a key). An app supplies only its panel (`EngineerChat`, `CardsPanel`), its own context (`appContext`) and its own API endpoints (`AgentApi::addAppEndpoint`). Do not write another per-app AI request path. An app lets the engineer act by registering tools and the state they change (`addTool`, `addStateDomain`; README there): each tool declares its effect (`read`, `write`, `destructive`, `external`), destructive and external ones ask the user in the app, and a request's changes undo together. Tool handlers call the same code the app's own UI calls - no back doors.

Its guidance is LiteSemRAG cards (`shared/LiteSemRag`, README there): rules, processes, tools, knowledge, personality, retrieved per request - by meaning (embeddings from the suite account) and by words, a card's own tokens first - so only what matches goes into the prompt. Cards live in the VFS, never in files on the OS: the suite's in `cards/suite.json`, each app's in `cards/app/<app>.json`, a project's inside the project (`Cards/cards.json`); shipped cards are built from source and read-only. A card id is one card across scopes and the closest scope wins (project, app, suite, shipped), so a project can sharpen or retire a suite rule. The user writes cards in the Cards panel; "Try a request" there shows which cards a request brings up.

Each running app's Virtual Engineer serves a local API (loopback, bearer token) announced as the VFS entry `agents/<app>.json`: talk to the engineer (`POST /v1/messages`, `GET /v1/requests/{id}`), see which cards a request would bring up (`GET /v1/cards/match?q=`), and look into the app (`GET /v1/app/{name}`). An agent working on the suite uses it instead of guessing at an app's state.

## Shared Ownership Rule

`shared/` is the highest-risk collision area in this workspace.

- Only one writable owner should modify `shared/` at a time.
- If another agent is doing suite-platform work, do not also edit `shared/` in the same checkout.
- Do not treat another agent's untracked files as authoritative without explicit confirmation.

## Multi-Agent Rule

Owner decision, October 17, 2026: **all LLMs share `D:\CreationSuite-Workspaces\DjehutiSuite` and its initialized submodules.** This replaces the former per-LLM clone/worktree arrangement.

- Do not recreate separate Claude, Codex, or Gemini Suite clones, worktrees, build trees, or executable drop areas for agent isolation. Reuse the shared project locations.
- Coordinate one writable owner per repository at a time. Another agent may inspect read-only; a takeover hands off the existing branch and working state instead of making another copy.
- Distinct application submodule repositories may have different owners when their work does not collide. Coordinate umbrella branch changes, `shared/`, gitlinks, and shared dependencies explicitly.
- Before switching branches, building, staging, or integrating, establish that no other agent is using that repository or build. Preserve and identify existing edits; never reset, clean, or absorb another agent's work silently.
- Use `docs/MULTI_AGENT_WORKFLOW.md` for the handoff and integration sequence. The machine-wide architecture briefing is `D:\CreationSuite-Workspaces\LLM_ARCHITECTURE_BRIEFING.md`.

### Shared LLVM installation

Reuse an existing verified LLVM installation through the build's configuration. Do not copy an LLVM installation for each LLM. The old personal checkout at `D:\000 Creation Suite` remains outside this shared source workspace; it is not an alternative agent checkout or a cleanup target.

Older build caches reference `D:\000 Creation Suite\apps\CreationEngine\vcpkg_installed`. Verify that dependency before configuring a new build. This documentation update does not relocate it. Do not build/rebuild LLVM, modify its installation, or invoke vcpkg in a way that might rebuild it without the explicit authorization required below.

### Task Branch and Completion Rule

Work on one focused feature branch for the authorized assignment, starting from the repository's current default branch. Do not implement changes directly on `main`/`master`, stack unrelated feature branches, or maintain separate standing development branches for each LLM.

An authorized implementation assignment includes its full Git closeout: appropriate build and focused checks, specific-file staging, commit and push the feature branch, merge into the repository's actual default branch, and push that default branch. Finish this cycle for each assignment. A context-only briefing or review-only request is not an implementation assignment. Respect an explicit user instruction to stop or withhold integration.

If repository protection requires a pull request, use that mechanism within the authorized scope; never disable protections, skip hooks, or force-push. If integration cannot be completed, report the concrete blocker and actual state rather than claiming the assignment is finished.

### Submodule Gitlink Sync Rule

A submodule's gitlink is a pinned commit, not a tracked branch — the umbrella repo does not "follow" `master` in `apps/CreationEngine` or any other submodule automatically. If you merge/pull inside a submodule and never come back to commit the resulting gitlink bump in the umbrella repo, the umbrella's `git status` will just say the submodule is "modified" with no detail, and it's easy to lose track of *why* -- this is the recurring cause of gitlink drift.

Configure once for this shared clone; subsequent agents reuse the same repository configuration:

```bash
git config status.submodulesummary 1
git config diff.submodule log
git config push.recurseSubmodules check
```

- `status.submodulesummary`/`diff.submodule=log` make `git status`/`git diff` in the umbrella repo actually show the commit range a submodule moved by, instead of a bare "modified" flag.
- `push.recurseSubmodules=check` refuses to push the umbrella repo if a submodule's checked-out commit isn't pushed to its own remote yet — catches the exact "gitlink points at a commit nobody else can see" bug before it happens.

`scripts/agent-git-check.sh` applies all three automatically (self-healing, not just a warning) and reports any submodule whose checkout has drifted ahead of the recorded gitlink — run it instead of eyeballing `git status` across the umbrella repo and its submodules.

Complete the submodule feature branch, push it, merge and push its default branch, then commit the resulting gitlink in the umbrella repository as part of the same assignment. Never publish an umbrella pointer to a local-only submodule commit. A documentation-only policy update may be backported to an intentionally older Suite source pin when the identical policy also lands on the app's default branch and the backport has a published remote ref. Preserve that app's existing source version; do not import unrelated application changes just to update agent instructions.

**FrustLang is always the latest: `third_party/FrustLang` follows FrustLang `master`.** Whenever a FrustLang PR merges to `master` — yours or anyone's, including research's — the next authorized Suite integration bumps `third_party/FrustLang` to the new `master` tip, rebuilt and smoke-tested against it. Before starting suite work that compiles FRust, compare the pin with `origin/master` (`git -C third_party/FrustLang fetch` then `git -C third_party/FrustLang log --oneline HEAD..origin/master`); anything listed is owed to the suite. Work done in FrustLang only lands in the suite through FrustLang `master`, so FrustLang work on any other branch must be merged and pushed to `master` before updating the Suite pin. Owner decision, 2026-10-02, after the pin sat at FrustLang #40 while #41 merged and later FrustLang work never reached `master`, so the suite built against an old FrustLang without anyone noticing.

## Secrets Directory Rule

The `Secrets/` directory contains critical credentials and private keys.
- `Secrets/` MUST remain in `.gitignore` at all times.
- **NEVER** stage, commit, or push any file inside `Secrets/` to any remote repository.

### Branch Ownership Check

Before editing or committing, inspect the current branch and working tree. A shared checkout retains the previous agent's state; an agent prefix is descriptive, not permission to switch away while that agent is active. Continue another agent's feature branch only through an explicit handoff. Start a new task branch only after the repository is available and its existing work is accounted for.

## Integration Rule

Serialize integration within each repository. The agent completing the authorized assignment performs its commit/push/merge/default-branch push under the Task Branch and Completion Rule; do not park finished work awaiting a separate integration assignment unless the user directs that.

For submodule work, finish and push the app's default-branch merge first. Then explicitly stage, commit, merge, and push the matching umbrella gitlink update. Coordinate umbrella ownership before changing that checkout. Keep unrelated edits and gitlinks out of the commit.

### Push Authorization Rule

The standing workflow authorizes pushing and merging the work in an assigned implementation task. Do not ask again merely because an old policy required per-action approval. This does not authorize unrelated repositories, deployments, credential changes, destructive cleanup, force-pushes, or new features. Stop and report a real authorization or repository-protection blocker when one remains.

## Commit Scope Rule

Before running `git commit` in ANY repo in this workspace — the umbrella repo, an app repo, or a wiki repo — run `git status --short` immediately before staging and stage only the specific files intended for that commit.

- Never assume the index is clean. Content staged earlier by another agent (or by you, earlier in a session) can silently ride along into an unrelated commit.
- Never use a broad `git add -A` / `git add .`.
- **Wiki commits contain wiki content only.** A commit made inside `wiki/` or any `apps/*/wiki/` checkout must never include changes from the paired code repo, and vice versa.
- If `git status` shows anything staged that you didn't just add yourself and can't explain, stop and ask before committing — don't push it along "because it was already there."
- This rule exists because it has already been violated once: a single new spec file committed via `git add <file>` (skipping a pre-commit `git status` check) swept in four unrelated submodule-pointer bumps that were already staged, and the resulting commit was pushed before anyone noticed. Submodule-pointer updates are the integrator's job per the Integration Rule above — they should never land as a side effect of an unrelated commit.

## Build Rule

Reuse the project's canonical build directory and outputs across all LLMs. Coordinate access and run one build at a time; do not create per-agent build folders, separate checkouts, or binary drops as the default answer to contention. Hand off or wait for the existing build owner.

### Build Configuration Rule

Use `Debug` builds by default for normal development, testing, and troubleshooting.

- `Debug` is the standard day-to-day development build.
- `Release` is only for intentional release work, such as a tagged version release or explicit release-build verification requested by the user.
- Do not assume `Release` just because a build is meant to be runnable.
- When reporting a build result, name the configuration you actually built.

### Single-Core Build Rule

Do not build with multi-core/parallel flags (`/m`, `--parallel`, `-j`) unless the user explicitly asks for a parallel build in that conversation. Build single-core by default — e.g. `cmake --build . --config Debug --target <Target> -- /m:1` for MSBuild-generated projects. This applies per-invocation and also means not running multiple app builds concurrently in the background at once; run builds one at a time. Requested directly by the user on 2026-08-22 after concurrent/multi-core background builds were making the machine sluggish during an active session.

### Build Output Context Rule

Never stream a verbose build's full compiler output into an AI conversation or tool result. Repetitive build progress and warning output consumes the session's context/token budget without providing useful information. Redirect the complete output to a log file inside the build directory and return only the process exit status. If the build fails, inspect and report only the relevant error lines plus the smallest amount of surrounding context needed to diagnose them. Await build completion without polling processes, timers, or unchanged logs. Inspect the log after completion only as needed to diagnose the result. Requested directly by the user on 2026-09-17 after streamed Station build warnings consumed a substantial part of the session budget.

### Shared Build Outputs

Use the application's existing canonical CMake output location as the executable location for every LLM. Do not add per-agent POST_BUILD copies or recreate agent-specific debug/release bin folders. If a shared distribution/staging area is specifically required, configure one shared location per configuration rather than one per agent. Keep installed runtime dependencies and toolchains separate from disposable build output.

### Focused Validation

Build the affected application target when code changes require it and check the function changed. Broad regression runs belong after a substantial completed section when the user directs them. Do not build applications to validate Markdown-only edits, run duplicate configurations without a reason, or claim a feature works solely because compilation passed.

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

## C: Drive Rule

**Under no circumstances write, download, build, or install anything on the C: drive. Period.** No file writes, no temp/cache directories, no package-manager installs (vcpkg, pip, npm, etc.) targeting any `C:\...` path — no exceptions for size, for "just this once," or for a tool's default location. Use a D:-drive path instead (the session's own D:-drive scratchpad directory for temp work; redirect any tool that defaults to C:\ via its env var/CLI flag/config before running it, don't run it first and find out where it wrote).

This is a structural rule, not a capacity judgment — it applies even if C: currently shows plenty of free space. It exists because on 2026-08-11, vcpkg operations writing to `C:\opt\vcpkg\...` filled the C: drive to 0 bytes free, which was the proximate cause of the LLVM incident described immediately below. If a task seems to require writing somewhere on C: with no D:-drive alternative, stop and ask the user rather than doing it anyway.

## LLVM / vcpkg Build Rule

**LLVM must never be built, rebuilt, or touched — directly or as a side effect of anything else — without an explicit, in-the-moment yes from the user in that exact conversation, no matter how the task is framed or how urgent it seems.** Same standard as the Push Authorization Rule above. Starting a build in the background and treating a same-turn status update as having asked does not satisfy this. If you are ever blocked on something that looks like it needs LLVM rebuilt, reconfigured, or otherwise touched, **stop and ask** — do not attempt it yourself, even if it looks safe, routine, or easily reversible.

The LLVM install at `apps/CreationEngine/vcpkg_installed/x64-windows/` is a custom, deliberately stripped-down (~11GB) build the user built by hand. It is not disposable, not a normal build artifact, and not something to "fix" if it looks broken, missing, or out of date — that appearance is not license to rebuild it.

**Indirect triggers are just as prohibited as running the build directly.** This has already happened once (2026-08-11): running `vcpkg install` in **manifest mode** (bare `vcpkg install`, or any invocation vcpkg treats as manifest mode, inside a directory containing a `vcpkg.json` that lists `llvm`) to add unrelated new dependencies caused vcpkg to reconcile the *entire* dependency list — including `llvm`, which nobody asked to touch — decide the existing install didn't match the manifest's expected ABI, and silently start a full from-source LLVM rebuild. That rebuild ran for over an hour, filled the disk to 0 bytes free, failed, and left the previously-working LLVM install removed and broken. Concretely:

- Never run `vcpkg install` in manifest mode anywhere `llvm` is a manifest dependency (this applies to `apps/CreationEngine/vcpkg.json` specifically, and any other manifest that lists `llvm`).
- To add a *different* vcpkg dependency to a project that also depends on `llvm`, install the new package(s) into a completely separate install root / separate `vcpkg_installed` tree that CMake points at independently (its own `CMAKE_PREFIX_PATH` entry) — never touch the shared manifest or the existing LLVM install root.
- Do not run `git fetch`/`git reset`/any history-rewriting operation on the vcpkg tool checkout itself to "fix" a compatibility error — that was part of the same chain of events.
- Before running any command that touches `vcpkg`, `Language/`, or FRust/LLVM build setup in Creation Engine, read `apps/CreationEngine/README.md`'s **"Scripting language build (LLVM via vcpkg)"** section first, in full, every time — see the Documentation Index below.

This rule has no exceptions and does not expire.

## No OS Dialogs Except Asset Import/Export Rule

**No app in this suite may open a native OS file/folder dialog except for explicit asset import/export** — importing a WAV, exporting a render, saving/loading a project-level file the user explicitly asked to save/load to disk. Anything that is "pick a thing the framework already knows about" (plugin/resource/asset selection where the app already maintains its own scanned or registered catalog) must be selected from that catalog only, via a menu/list built from it — never via `juce::FileChooser` or any other native browse-for-file/folder dialog.

Violated on 2026-08-11 in `apps/CreationStation`: the Signal Graph's "VST Host" node's plugin picker (`showPluginLoadMenu()` in `MainComponent.cpp`) fell back to, and always additionally offered via a "Browse manually..." item, a raw OS file dialog to browse for a `.vst3` DLL directly — instead of exclusively listing the app's own pre-scanned `vstPluginCatalog` entries. User: "this system when it comes up it scans VST3s and has the loaded onto a host. If you want to use a VST you do not open a OS dialog, you tell the framework what VST you want to use... this is a violation of the perimeter." This rule had been stated verbally before but was never written into any doc in this repo — checked every `AGENTS.md` and every `.md` file across all branches and the app submodule's own history to confirm. It is written here now specifically so that gap can't recur.

Before wiring any "choose an X" UI in any app, check whether the framework already has a scanned/registered list for X. If it does, the picker must be a menu/list over that catalog only — rescan-the-catalog and manage-search-paths actions are fine (they configure *where* the framework looks), but a direct file/DLL/plugin/asset browse dialog is not. Reserve native file dialogs strictly for user-initiated import of external content into a project or export of project content out to disk.

## Storage Boundary Rule: OS filesystem vs. the VFS

**OS-level (per-machine) storage is limited to exactly one thing: the VFS root pointer** — `suite-settings.json` (`%APPDATA%\Djehuti-Suite\suite-settings.json`, via `creation::suite::SuiteSettingsStore`), a single `suiteVfsRoot` field. Even the service's process registry (`SuiteProcessRegistry`, how apps find or start `CreationSuiteVfsService.exe`) lives inside the VFS root. One place for everything is the point: per-app folders in AppData scatter copies of settings and keys across the machine, and nothing knows which copy is current. Nothing else belongs on the raw OS filesystem.

**There is no default root and no fallback location.** With no root chosen, every `creation::suite::get*Directory()` returns an empty `juce::File`, and `SuiteJUCEApplication` asks the user for a folder at startup and exits if none is chosen. The root folder holds exactly two files: `vfs.bin`, the one container that every project, asset, setting, layout, mapping, log and cache lives inside, and `VfsHeartbeat.json`, which the VFS service writes so apps can find it. No subfolders, and no files written by apps (`docs/architecture/Suite-VFS-Single-Container-Plan.md`). Code never uses the OS temp folder (`juce::File::tempDirectory`). The only file the suite writes to the OS is the root pointer. Older folder helpers (`getCurrentScratchDirectory`, `getLogsDirectory`, `getCacheDirectory`, `getProjectContainerDirectory`) and the asset copy-out (`AssetMaterializer`) still exist in code; they are the plan's unfinished phases 2 and 3, not a pattern to use.

**Everything else — app settings, control-surface/device mappings, window layout, any future per-app config, in any app — goes through the suite's VFS service**, never a raw local file. The service is real and functional (`services/VfsService`, HTTP on localhost). Use `creation::services::SuiteVfsServiceClient` (`shared/AssetSystem/include/creation/services/SuiteVfsServiceClient.h`) directly, or — for anything JSON-shaped, which is almost everything — `creation::services::SuiteVfsJsonStore` (`shared/Services/include/creation/services/SuiteVfsJsonStore.h`), which factors out the discover+readEntry+parse / discover+writeEntry+serialize boilerplate. The suite's own entries (AI accounts and keys, AI health and diagnostics, the activity log) are listed in `docs/architecture/Suite-Shared-Project-Model.md`; AI keys are entered once in the suite's settings and every app reads them from `ai-settings.json` — no app asks for or stores a key. Tests use `SuiteVfsJsonStore::setScopeForTesting` so they never read or overwrite the real entries. `SuiteAiSettingsStore` (`shared/Services/src/SuiteAiSettings.cpp`) is the reference example.

Violated and fixed 2026-08-11: CreationStation's own settings (`settings.xml`), control-surface mappings, and window layout, plus CreationEngine's settings file, were all raw OS files written via `juce::File`/`XmlElement`, entirely bypassing the VFS — this is exactly why a user's ~10 configured VST plugin search folders silently vanished (the write silently no-op'd whenever the VFS root wasn't yet configured at save time, with no error surfaced). All four were migrated to `SuiteVfsJsonStore`-backed VFS entries in that pass; treat that migration as the concrete precedent for how to do this correctly. **Before adding any new local persistence in any app, check whether `SuiteVfsJsonStore` already covers the need — it almost always does.** This is the same failure category as the LLVM/C: drive rules above: the suite is pre-release, under construction — there is no reason to build app-local storage "for now" instead of doing it correctly the first time.

## The ":STUPID" Rule: stop and talk

When you find something that is plainly wrong for this architecture - code that touches the OS file system instead of the VFS, a click handler that copies or opens a huge file instead of reading the manifest, a leftover from the app's early days that nobody removed, a workaround built on top of a workaround - that is a `:STUPID`. Do not quietly patch around it, do not rewrite a large piece of shared code in one go, and do not carry on with the task as if it were fine.

**Stop, and talk to the user first.** Say in plain words what you found, why it is wrong, and what you would change. Then wait. The user decides the order and scope.

Why this exists: real examples include a details panel that copied a 2 GB video out of the VFS on every click (a 20 second wait), a raw file-system browser mode kept in the shared Assets window, and a storage folder that fell back to a default on the C: drive. Each one was found by the user, not by the agent, and each was made worse by moving on without saying anything. There is no legacy to protect: greenfield rules apply, and replaced things are deleted outright, but only after the user has agreed to the plan.

## Documentation Index

This file is a MAP, not a manual — it tells you which document governs a given topic; it does not restate that document's content. Before starting work in an area below, open the linked file(s) and read them, live, every session — do not rely on a memory of having read them before, and do not let anything here substitute for actually opening the file.

**Suite-wide:**
- Local ecosystem orientation → `D:\CreationSuite-Workspaces\LLM_ARCHITECTURE_BRIEFING.md`; current folder inventory → `D:\CreationSuite-Workspaces\DRIVE_REPOSITORY_MAP.md`.
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
- **Node graph symbols - params, constants, variables - are a suite standard: every node-based system supports them the same way** → [shared/NodeSystem/SYMBOLS.md](shared/NodeSystem/SYMBOLS.md). Read it before building or changing any node graph system; it follows the research node language schema (FrustLang `projects/10_node_compiler/NODE_SCHEMATIC_SCHEMA_V2.md`). Owner decision, 2026-09-30.
- **One node system: a graph has a type that picks its nodes, a graph can be used as a node in another graph, and decisions are data-flow Switch / Route nodes** → [shared/NodeSystem/GRAPH_TYPES.md](shared/NodeSystem/GRAPH_TYPES.md) and [shared/NodeSystem/FLOW.md](shared/NodeSystem/FLOW.md); types (enums, then structs) are made in type editors, not in graphs, and are usable wherever they are in scope (graph / project / pod) → [shared/NodeSystem/TYPES.md](shared/NodeSystem/TYPES.md). Read them before building or changing any node graph system. Owner decisions, 2026-10-01.
- Each `shared/*` library has its own `README.md` (`shared/README.md` for the overview, then `shared/AssetSystem`, `FRust`, `shared/CMake`, `shared/Interop`, `shared/NodeSystem`, `shared/Services`, `shared/UI`) — read the specific one before changing that library.
- Suite VFS Browser (debug-only tool, suite Settings panel → "VFS Browser" tab, `#if JUCE_DEBUG`-gated) → [docs/Suite-VFS-Browser-Debug-Tool.md](docs/Suite-VFS-Browser-Debug-Tool.md) — what it shows, where it lives, known limitations.
- FRust language design (types, memory model, functional-paradigm features, the real-time-safe execution profile, modules) → [FRust/docs/FRust_V2_LANGUAGE_SPEC.md](FRust/docs/FRust_V2_LANGUAGE_SPEC.md) — read before touching FRust's grammar, sema, or intrinsic surface; tracked as GitHub issues #49–#59 on Creation-Suite.
- Hermes (FRust package manager/distribution, LagDaemon.com store) → [FRust/docs/HERMES_PACKAGE_MANAGER_SPEC.md](FRust/docs/HERMES_PACKAGE_MANAGER_SPEC.md) — quick-capture spec, not yet filed as issues; read before scoping any package-management work.

**Per app** (`apps/CreationEngine`, `apps/CreationMovie`, `apps/CreationStation`, `apps/CreationLive`): each has its own `AGENTS.md` (app-local policy, binding alongside this file per Scope above), `README.md` (build/setup instructions — e.g. Engine's LLVM/vcpkg section), and `docs/` folder. Always check for and read an app's own `README.md`/`AGENTS.md`/`docs/` before working in it; do not assume this index is exhaustive of what's there. Known app-specific docs as of this writing:
- Creation Engine: [docs/CAPABILITIES.md](apps/CreationEngine/docs/CAPABILITIES.md) (scope/non-goals), [docs/GS_SCRIPTING_PLAN.md](apps/CreationEngine/docs/GS_SCRIPTING_PLAN.md) (FRust milestone plan), [docs/SCRIPTING_ABI.md](apps/CreationEngine/docs/SCRIPTING_ABI.md) (host ABI spec), [docs/CROSS_APP_LANGUAGE_DOMAINS.md](apps/CreationEngine/docs/CROSS_APP_LANGUAGE_DOMAINS.md) (intrinsic domain gating), [docs/CROSS_APP_ASSET_INTEROP.md](apps/CreationEngine/docs/CROSS_APP_ASSET_INTEROP.md)
- Creation Movie: [docs/CAPABILITIES.md](apps/CreationMovie/docs/CAPABILITIES.md), [docs/LANGUAGE_ROLLOUT.md](apps/CreationMovie/docs/LANGUAGE_ROLLOUT.md)
- Creation Station: [docs/Creation-Shared-Language-Rollout.md](apps/CreationStation/docs/Creation-Shared-Language-Rollout.md), [docs/Creation-Suite-AssetSystem-Standard.md](apps/CreationStation/docs/Creation-Suite-AssetSystem-Standard.md), [docs/Creation-Suite-Interop-Spec.md](apps/CreationStation/docs/Creation-Suite-Interop-Spec.md), [docs/Studio-Grade-Audio-Routing-Checklist.md](apps/CreationStation/docs/Studio-Grade-Audio-Routing-Checklist.md), [docs/Creation-Station-0.5.0-Beta-Checklist.md](apps/CreationStation/docs/Creation-Station-0.5.0-Beta-Checklist.md), [docs/STATION_CONTROL_REGISTRY_SEED.md](apps/CreationStation/docs/STATION_CONTROL_REGISTRY_SEED.md) — concrete set_state/get_state entries for the FRust control registry, seeded from a real codebase investigation; [docs/Signal-Lab-Character-Macros-Spec.md](apps/CreationStation/docs/Signal-Lab-Character-Macros-Spec.md) — Signal Lab's high-level sound-character macro definitions and guardrails; [docs/Signal-Lab-Node-Graph-Spec.md](apps/CreationStation/docs/Signal-Lab-Node-Graph-Spec.md) — living spec for Signal Lab's node-graph system (port anatomy, connections, runtime model, Sources/Sinks); read before touching node/port/connection code, and keep it updated as requirements are given or changed

**Task/status tracking:** GitHub Issues + the **Creation Suite Road Map** project board (Project #19) — see the GitHub Project Board Rule above, not a doc file.

**Keeping this index honest:** if you find a document that governs a topic covered by a task you're doing, and it isn't listed here, add it here as part of that task — this index is only useful if it stays current. Conversely, if a linked file no longer exists or has been superseded, fix or remove the entry rather than leaving a dead pointer.
