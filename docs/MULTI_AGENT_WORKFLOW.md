# Creation Suite Multi-Agent Workflow

Date: July 29, 2026

This document defines how Codex, Claude, Gemini, or any other agent should work in parallel on the Creation Suite without corrupting each other's checkouts, creating merge confusion, or reporting false completion.

## Bottom Line

Use **one owner per repo/worktree at a time**.

Do not point multiple agents at the same writable checkout unless one of them is strictly read-only.

If multiple agents need to work in parallel, give them:

- different repositories, or
- different git worktrees of the same repository, or
- clearly separated phases where only one agent is allowed to commit/integrate

## The Main Problem We Must Avoid

Agents are fast, but git state is literal.

If two agents share one writable checkout, they will see:

- each other's untracked files
- half-finished edits
- stale build outputs
- branch confusion
- false "already migrated" or "missing" conclusions

That is not a model problem. It is a workspace-coordination problem.

## Required Operating Model

### Rule 1: One Writable Owner Per Checkout

At any moment, each checkout or worktree must have exactly one writable owner:

- one agent, or
- one human

Everyone else is read-only until handoff.

### Rule 2: Prefer Separate Worktrees For Parallel Work In The Same Repo

If two agents need to touch the same repo, do not share one working directory.

Use separate git worktrees or separate clones so each agent gets:

- its own branch
- its own untracked files
- its own build folder
- its own scratch state

### Rule 3: One Integrator

Only one actor should do integration merges at a time.

That integrator can be:

- you
- Codex
- another designated agent

But it must be explicit.

The integrator is the only one who should:

- merge branches
- resolve conflicts
- update the umbrella repo's submodule pointers
- declare a feature branch "landed"

### Rule 4: Shared Suite Code Has Explicit Ownership

`shared/` is the highest-risk collision zone in this workspace.

Only one agent should actively modify `shared/` at a time unless the work is split into different worktrees and intentionally coordinated.

If one agent is doing suite-platform work, other agents should stay inside app-local repos unless told otherwise.

## Recommended Setup For This Suite

### Best Setup

Use this pattern:

- one agent owns `shared/` and umbrella-repo integration
- one agent owns `apps/CreationMovie`
- one agent owns `apps/CreationTexture`
- one agent owns another app repo only if that repo is isolated from the current shared-platform work

### For Same-Repo Parallelism

If two agents must work on `Creation Suite` itself:

1. create a separate worktree per agent
2. assign one branch per worktree
3. assign one clear ownership area per branch
4. do not merge directly from one agent's worktree into another by hand-copying files
5. integrate only through git commits and an explicit merge step

## Branching Rules

### Naming

Use clear branch names with an owner/topic shape, for example:

- `codex/frust-suite-cutover`
- `claude/movie-transport-pass`
- `gemini/texture-domain-spec`

### Scope

Each branch should have one main purpose.

Good:

- shared FRust migration
- Movie shell fixes
- Station remote-control MVP planning

Bad:

- "misc cleanup"
- "suite stuff"
- "a bunch of fixes"

### Branch Ownership

Once an agent starts writing on a branch, no other agent should also write to that same branch unless you intentionally hand it off.

## Merge Rules

### When To Merge

Merge when all of these are true:

- the branch has a single coherent purpose
- the branch builds or is documented as blocked by one explicit dependency
- the affected tests or smoke checks were run when possible
- the branch is not hiding unrelated local edits
- the integrator has reviewed the actual diff

### When Not To Merge

Do not merge when:

- the branch contains unrelated experiments
- the branch depends on uncommitted local files
- the branch is only "working in this one checkout"
- another agent is actively editing the same target files
- the build is red for unknown reasons

### Merge Direction In This Suite

For app-local work:

- merge inside the app repo first
- then update the umbrella repo's submodule pointer

For suite-shared work:

- merge in the umbrella repo
- then cut app repos over to consume the shared result

Do not silently do both at once without saying so.

## Handoff Rules

When one agent hands work to another, the handoff must say:

- repo
- branch
- files touched
- what is complete
- what is still risky
- what was verified
- what was not verified

Short example:

`Repo: Creation Suite`
`Branch: codex/frust-suite-cutover`
`Touched: FRust, shared/NodeSystem, wiki/Roadmap.md`
`Done: suite FRust targets wired`
`Risk: LLVM environment still rebuilding`
`Verified: configure logic updated, source copied`
`Not verified: clean JIT build yet`

## Build Folder Rules

Build outputs are the second major collision source after source edits.

Rules:

- do not share one scratch build folder across agents
- if a repo already has a user-mandated build directory, only one writable owner should use it at a time
- if parallel work is necessary, the parallel agent must use its own worktree-local build directory unless project instructions forbid it

For this suite specifically:

- respect any per-app `AGENTS.md` rule about fixed build directories
- use `Debug` builds for normal development work
- reserve `Release` builds for tagged release work or explicit release verification
- if that rule prevents safe parallel builds, then parallel writable work in that repo should not happen in the same checkout

## Issue And Board Rules

Tasks must be managed on the official GitHub Project Board (**Creation Suite Road Map** - Project #19) using real GitHub Issues created via `gh issue create` and linked via `gh project item-add 19`.

- **Shorthand Definition**: Whenever the user says "create a task", "add tasks", "track tasks", or similar, it ALWAYS means: *"Create real GitHub Issues on the target repository (`gh issue create`) and add them directly to the **Creation Suite Road Map** project board (`gh project item-add 19 --owner wwestlake --url <issue-url>`)"*.
- **Status Sync Rule**: Every time an agent completes a task, fixes an issue, or changes work status, the agent MUST immediately update the corresponding GitHub Issue and Project Board item on **Creation Suite Road Map** (Project #19), adding completion comments and marking status ready for testing.
- Do not rely on local scratch `.md` files for task management.
- Before updating roadmap/board status:
  - inspect current code
  - inspect current branch state
  - inspect whether work is committed or only local
- Explicitly tag completed work ready for testing directly on the corresponding GitHub issue and Project Board item.
- Do not mark something complete only because:
  - one agent said it was done
  - a branch exists
  - files were copied
  - one partial build once worked in another checkout

## Recommended Workflow For You Right Now

For the current team setup, the least painful process is:

1. assign one agent as suite integrator
2. keep Movie, Texture, and other app-local work on separate agents
3. keep shared FRust / shared platform work on one agent only
4. let app-local agents merge only inside their app repos unless told otherwise
5. have the suite integrator update the umbrella repo and submodule pointers after review

## Simple Policy You Can Paste To Other Agents

Use this workspace policy:

`You are not the integrator unless explicitly told otherwise.`

`Do not edit shared/ if another agent owns suite-platform work.`

`Do not use the same writable checkout as another agent.`

`Do not merge anything outside your assigned repo or branch.`

`If you finish app-local work, stop at a clean branch and report repo, branch, files, verification, and risks.`

## Current Recommendation

For the Creation Suite as of July 29, 2026:

- keep one agent on suite/FRust integration
- keep other agents on isolated app repos only
- do not let multiple agents write to the umbrella repo working tree at the same time
- do not treat untracked files in a shared checkout as authoritative work unless the owning agent confirms them
