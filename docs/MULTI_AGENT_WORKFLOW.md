# Creation Suite Shared LLM Workspace

Updated October 17, 2026, by the owner's shared-workspace decision.

## Shared location

Every LLM uses `D:\CreationSuite-Workspaces\DjehutiSuite`. The umbrella contains the initialized application and dependency submodules. Switching between Claude, Codex, Gemini, or another LLM does not create a new clone, worktree, dependency installation, build tree, or executable drop area.

This replaces the July policy of separate worktrees and the later per-LLM clone/bin arrangement. The machine-wide architecture briefing is `D:\CreationSuite-Workspaces\LLM_ARCHITECTURE_BRIEFING.md`; the location inventory is `D:\CreationSuite-Workspaces\DRIVE_REPOSITORY_MAP.md`.

## Current authority and review boundary (October 25, 2026)

Codex is the Executive Director's designated working agent for the entire Suite
repository structure until reassigned. No ownership negotiations or LLM handoffs
are required under that assignment. Existing unfinished work must be preserved.

Use clean integrated `master`, then a fresh task-specific branch. Implement and
test, commit and open a PR to `master` for Executive Director review/merge. Do not
bypass required checks/reviews. Only delete the branch after its actual merge;
finish dependent submodule/gitlink PRs and verify clean states. An open PR is not
completed integration. These instructions supersede conflicting transfer and
direct/automatic merge steps below; the single shared checkout/build policy stays.

## Coordinate ownership

- One writable owner uses a given repository at a time. Others can inspect read-only. Transfer ownership explicitly before changing its branch, index, files, or build state.
- Different independent application submodules may be assigned to different agents when their changes do not overlap. The umbrella branch, shared code, dependency state, and submodule-pointer updates require coordination.
- Do not switch the umbrella branch or run recursive submodule updates while app owners have active work.
- Inspect existing changes before beginning. Preserve unfinished edits, identify their owner, and hand them off; never reset/clean/stage them as an incidental part of another assignment.
- Resolve contention by handing off or sequencing work in the existing checkout. Another per-agent clone/build area is not the default solution.

## Feature branches and completion

Start each authorized implementation assignment on one focused feature branch from the repository's current default branch. An owner/topic name is useful; a standing personal integration branch is unnecessary. Do not implement directly on `main`/`master` or stack unfinished features.

Finish each assignment with this cycle:

1. Inspect and implement only its authorized scope.
2. Run the appropriate build and minimal functional check for what changed. Documentation-only work needs text/link checks, not an application build.
3. Inspect status and the actual diff; stage only the intended files, commit, and push the feature branch.
4. Merge into the repository's actual default branch and push that branch in the same assignment. Use a PR if repository protection requires it; do not bypass protection or hooks.
5. For a changed app submodule, finish its default-branch push first, then explicitly commit, merge, and push the corresponding umbrella gitlink update.
6. Confirm the resulting default branch is on the server, record the actual checks and remaining limits in the relevant handoff, and stop.

The standing task workflow includes integration authorization for the assignment. Do not leave completed work parked on agent branches or ask again solely because an older policy gated each push/merge. An explicit review-only/context-only request, pause, or instruction to withhold integration still governs. Never force-push or include unrelated staged changes. Report a concrete blocker if closeout cannot finish; do not claim it is complete.

Only one agent integrates a repository at a time. The agent closing its authorized assignment performs that integration after coordinating ownership; a separate permanent integrator assignment is not needed for every feature.

For a documentation-only update on an older Suite app pin, publish the identical policy on the app's default branch and on a version-preserving backport with a remote ref. The umbrella may retain that backported source pin. Do not adopt unrelated app changes just to update instructions, and never reference an unavailable local-only commit.

## Shared builds and dependencies

Reuse canonical project build/output locations across LLMs. Only one build runs at a time on this machine, single-core (`--parallel 1` or `/m:1`). Use Debug for development and diagnostics, Release for distribution and installers. Configuration separation is valid; per-agent duplication is retired.

Do not add POST_BUILD copies into agent-named bin folders. Use the application's canonical output directly; create a single shared staging area per configuration only when the assignment actually requires one.

Reuse verified installed dependencies through configuration. Do not copy LLVM per LLM or let a package command rebuild it implicitly. Its specific authorization and protection rules remain in `AGENTS.md`. This documentation change does not relocate installed dependencies or reconfigure application builds.

Redirect compiler output to a log and await completion. Do not poll build processes/timers/logs. On failure, inspect the relevant diagnostics. Check the changed function; run broad regression only after a substantial section when the user directs it.

## Handoff

An agent taking over receives the same checkout and a concise record of:

- the current assignment and scope;
- repository, branch, working edits, and any staged files;
- what is complete, pushed, and merged, versus still local;
- relevant build configuration, canonical output, and any running operation;
- actual functional checks and unresolved limitations;
- the next action and who owns each involved repository.

Read the current files and handoff before continuing. Another agent's report is evidence to check, not permission to overwrite its work. Commit identifiers can be recorded internally, but user-facing summaries should name the feature and result.

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
