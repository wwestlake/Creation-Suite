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

**`D:\000 Creation Suite` is the user's own personal clone. No LLM may read, explore, build in, or otherwise touch it, for any reason.** It is not a fallback, not a reference copy, not a shared workspace — treat it as off-limits, full stop.

Do not use the same writable checkout as another agent. Never assume that untracked files, half-finished edits, or local build outputs in another agent's checkout are safe to build on — each agent's checkout is independent and may be ahead, behind, or diverged from any other at any time. To get current shared code, `git pull`/`git fetch` your own clone against `origin` — never by reading another agent's directory.

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

`git push` is never inferred. It requires an explicit, in-the-moment instruction to push, from the user, in that exact conversation — not:

- a request that would merely be *satisfied* by something existing on the remote (e.g. "give me a link to that file" does not mean "push it")
- a prior approval to push something else, earlier or elsewhere
- the fact that a task feels finished or ready

If completing a request seems to require a push and you are not the designated integrator for that repo, **stop and say so explicitly** before doing it: name the repo/branch, say a push is needed to accomplish the request, and ask. Do not act first and explain afterward. This applies even to low-risk-looking pushes (a single new doc file, a wiki page) — risk level does not change the authorization requirement.

This rule exists because it was already violated: a request for "a link I can paste into a ticket" was read as implicit permission to commit and push, which it was not, and the resulting push also carried unrelated staged submodule-pointer changes along with it (see Commit Scope Rule below).

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
- Shared-library extraction plan/status → [docs/SHARED_EXTRACTION_PLAN.md](docs/SHARED_EXTRACTION_PLAN.md), [docs/SHARED_INFRASTRUCTURE_AUDIT_2026-07-28.md](docs/SHARED_INFRASTRUCTURE_AUDIT_2026-07-28.md)
- Asset/VFS storage standard → [docs/standards/Suite-Asset-VFS-Standard.md](docs/standards/Suite-Asset-VFS-Standard.md)
- Project template scaffolding → [docs/Project-Template-Generator.md](docs/Project-Template-Generator.md), [docs/Suite-Project-Template-LLM-Instructions.md](docs/Suite-Project-Template-LLM-Instructions.md)
- Cross-app migration history/plans → [docs/migration/Migration-Plan.md](docs/migration/Migration-Plan.md), [docs/migration/Station-Engine-First-Moves.md](docs/migration/Station-Engine-First-Moves.md)
- LLM/agent infrastructure guide → [docs/LLM-INFRASTRUCTURE-GUIDE.md](docs/LLM-INFRASTRUCTURE-GUIDE.md)
- Each `shared/*` library has its own `README.md` (`shared/README.md` for the overview, then `shared/AssetSystem`, `shared/CEL`, `shared/CMake`, `shared/Interop`, `shared/NodeSystem`, `shared/Services`, `shared/UI`) — read the specific one before changing that library.

**Per app** (`apps/CreationEngine`, `apps/CreationMovie`, `apps/CreationStation`, `apps/CreationLive`): each has its own `AGENTS.md` (app-local policy, binding alongside this file per Scope above), `README.md` (build/setup instructions — e.g. Engine's LLVM/vcpkg section), and `docs/` folder. Always check for and read an app's own `README.md`/`AGENTS.md`/`docs/` before working in it; do not assume this index is exhaustive of what's there. Known app-specific docs as of this writing:
- Creation Engine: [docs/CAPABILITIES.md](apps/CreationEngine/docs/CAPABILITIES.md) (scope/non-goals), [docs/GS_SCRIPTING_PLAN.md](apps/CreationEngine/docs/GS_SCRIPTING_PLAN.md) (CEL milestone plan), [docs/SCRIPTING_ABI.md](apps/CreationEngine/docs/SCRIPTING_ABI.md) (host ABI spec), [docs/CROSS_APP_LANGUAGE_DOMAINS.md](apps/CreationEngine/docs/CROSS_APP_LANGUAGE_DOMAINS.md) (intrinsic domain gating), [docs/CROSS_APP_ASSET_INTEROP.md](apps/CreationEngine/docs/CROSS_APP_ASSET_INTEROP.md)
- Creation Movie: [docs/CAPABILITIES.md](apps/CreationMovie/docs/CAPABILITIES.md), [docs/LANGUAGE_ROLLOUT.md](apps/CreationMovie/docs/LANGUAGE_ROLLOUT.md)
- Creation Station: [docs/Creation-Shared-Language-Rollout.md](apps/CreationStation/docs/Creation-Shared-Language-Rollout.md), [docs/Creation-Suite-AssetSystem-Standard.md](apps/CreationStation/docs/Creation-Suite-AssetSystem-Standard.md), [docs/Creation-Suite-Interop-Spec.md](apps/CreationStation/docs/Creation-Suite-Interop-Spec.md), [docs/Studio-Grade-Audio-Routing-Checklist.md](apps/CreationStation/docs/Studio-Grade-Audio-Routing-Checklist.md), [docs/Creation-Station-0.5.0-Beta-Checklist.md](apps/CreationStation/docs/Creation-Station-0.5.0-Beta-Checklist.md)

**Task/status tracking:** GitHub Issues + the **Creation Suite Road Map** project board (Project #19) — see the GitHub Project Board Rule above, not a doc file.

**Keeping this index honest:** if you find a document that governs a topic covered by a task you're doing, and it isn't listed here, add it here as part of that task — this index is only useful if it stays current. Conversely, if a linked file no longer exists or has been superseded, fix or remove the entry rather than leaving a dead pointer.
