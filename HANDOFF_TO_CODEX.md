# Handoff to Codex — Claude session, 2026-08-13 through 2026-08-14

Date: 2026-08-14
From: Claude, branch `claude/development` (superproject + `apps/CreationStation`,
`apps/CreationMovie`, `apps/CreationModeler`, `apps/CreationTexture`)

This supersedes the previous handoff in this file (2026-08-04, VFS service bring-up —
that work is long since merged and built on top of). This one covers a much longer,
multi-day stretch of work. Everything below is committed and pushed; open PRs are
listed at the bottom.

## Open PRs (review/merge these first)

- **Creation-Suite#79** — superproject + shared libraries (VFS storage overhaul,
  shared Timeline/NodeEditorUI/Video groundwork, Suite AI Accounts redesign).
- **CreationStation#44** — app-level changes riding on the above (this is the biggest
  one; title/body updated this session to reflect the full accumulated scope, not
  just its original two-bug-fix description).
- **Creation-Movie#30**, **Creation-Modeler#2**, **Creation-Texture#2** — smaller,
  mostly FRust + shared/NodeSystem wiring and suite AI plumbing for those three
  apps, plus `onProjectOpenRequested` wiring so their Open Project button actually
  works.

`apps/CreationEngine` and `apps/CreationLive` don't have open PRs — their work this
stretch (Engine's real-folder VFS project storage migration; Live's suite-shell
wiring: header, settings, Open/New Project) was already committed directly to
their own default branches (`master`/`main`) by an earlier session, not to
`claude/development`, so it's already merged as of this push - nothing pending
review there.

**Note for whoever pulls this repo next**: those two submodules' commits existed
only in this local checkout until this session pushed them (see "Dangling
submodule gitlinks" below) - if you'd tried `git pull --ff-only` before this push,
you'd have hit `upload-pack: not our ref` for both. That's fixed now; if you see
it again, the fix is: `cd apps/<app>; git push origin <its-default-branch>`.

## Dangling submodule gitlinks (found and fixed this session)

The superproject's tree can reference a submodule commit that only exists in
*someone's local checkout* if that commit was never pushed - `git submodule
update`/`git pull --ff-only` on any other clone then fails with
`upload-pack: not our ref <sha>` for that submodule, since the remote genuinely
doesn't have it. Found and fixed two instances this session (pre-existing, not
introduced this stretch): `apps/CreationEngine`'s `master` and
`apps/CreationLive`'s `main` were both sitting one commit (Engine) or two commits
(Live) ahead of their own remotes, unpushed, and the superproject already
referenced those local-only SHAs. Fixed by pushing both (clean fast-forwards,
verified via `git merge-base --is-ancestor` before pushing - no force needed).

Also found and removed a genuinely broken, unrelated gitlink:
`third_party/asio-sdk` was a `160000` tree entry with **no corresponding
`.gitmodules` entry, ever** (checked full history) and an empty directory on
disk - a dangling reference from some earlier accidental `git add` of a nested
repo, not a real submodule. Removed via `git rm --cached`.

If you hit `upload-pack: not our ref` or `no submodule mapping found in
.gitmodules` again: `git ls-tree HEAD <path>` to see what commit the superproject
expects, then in that submodule check `git merge-base --is-ancestor <that-sha>
origin/<branch>` - if it says NO, someone has unpushed local work the superproject
already depends on; push it (after confirming it's a real fast-forward, not
diverged history).

## What landed, by area

### 1. VFS storage overhaul (packed containers → real folders)

Projects used to be a single packed `.csproj` container (FatFs-backed virtual disk
image). That's gone for the paths that matter:

- New `VfsProjectStore` + `/project/*` endpoints in `services/VfsService` — projects
  are real folders under the configured VFS root, keyed by project id.
- `SuiteVfsServiceClient` gained project create/list/clone/entry methods.
- `ProjectSession`/`ProjectContainerService`/`ProjectWorkspaceService`/
  `AssetMaterializer` reshaped to call the service instead of `SuiteVolume`/FatFs.
- App call sites migrated: CreationEngine, CreationMovie, CreationStation,
  `SuiteShellController`'s project browser.
- App settings, control-surface mappings, and window layout also migrated off raw
  OS files onto the VFS service (`SuiteVfsJsonStore`).
- A debug-only VFS Browser tab (JUCE_DEBUG-gated) was added to the suite Settings
  window with a pretty-printed JSON/XML content viewer.

**Not done yet** (paused, tracked as Stage 5/6 in earlier session notes, and now
also noted on **Creation-Suite#63**): `shared/VFS`/FatFs/`SuiteRootProject`/
`SuiteProjectHandoff` still exist as dead code, not yet deleted. And the actual
point of #63 — multiple app processes with the *same* project open at once, seeing
each other's writes live, with real write-coordination — is **not built**. Today
each app still opens its own `ProjectSession` independently; nothing arbitrates
concurrent writes from two processes to the same project. That's still open.

### 2. Tracker toolbar redesign (CreationStation)

Seven-item list, all landed: removed redundant zoom buttons (Ctrl+scroll already
zoomed), fixed the grid-resolution dropdown (three separate root causes), replaced
the fixed-octave Pitch Pipe popup with note/octave dropdowns + Play, added a new
shared `NamedAssetSaveLoadMenu` component and standardized Save/Load onto it,
right-click marker placement/delete + drag-to-move, per-track drag-to-resize
height. Plus: Ctrl+click Rewind/Fast-Forward jumps to timeline start/end (shared
`CreationSuiteHeaderBar`, additive/opt-in), X-Touch scrub keeps the playhead
centered while the timeline scrolls under it.

### 3. Named, saveable, reloadable tool objects

`ProjectAssetService::saveGeneratedAsset` (new shared mechanism) + a new
`AssetKind::trackerArrangement` — Tracker arrangements, Signal Lab patches, and
Foley setups are all now named project assets, saved/loaded through one shared
path instead of each tool reinventing it. Each tool's last-active saved object
auto-restores on project open.

### 4. Foley / video redesign (CreationStation)

The legacy Foley Stage (`ArrangeView`) was retired outright. Replaced with:

- Video tracks in Tracker: a real hardware-decode pipeline (D3D11 + Media
  Foundation, NV12 texture → BT.709 HLSL compositing — deliberately **not**
  `juce_video`), thumbnail-strip rendering, scrub preview overlay.
- A new node-graph-driven Foley setup (trigger/PlaySample/gain/Sequence/Branch/
  random-select/Delay nodes), FRust codegen, saved as a named project asset.
- Shared groundwork for this: `shared/Timeline` (generic track/clip core, pulled
  out of CreationStation), `shared/NodeEditorUI` (generic node-graph canvas/
  inspector/palette, pulled out of CreationEngine), `shared/Video` (native D3D11
  HLSL video-FX node infrastructure — `ColorBalanceNode`/`GaussianBlurNode`/
  `TransformCropNode`, verified via a WARP-device smoke test), and OpenFX host
  skeletons (`shared/OfxHost` — property/param/image-effect suites implemented and
  smoke-tested, but plugin discovery / zero-copy `IDXGIResource` texture sharing /
  real third-party `.ofx` plugin loading are explicitly **not implemented** — no
  real OFX plugin binary was available in this environment to test against).

**Not manually verified**: dropping a real video file on a video track and
confirming scrub/split/move/trim/preview actually work end-to-end (compiles and
links against the real SDK; genuine hardware-decode correctness needs a human at
the running app with a real GPU and a real video file).

### 5. First-party plugin auto-scan

Fixed two real CMake bugs (POST_BUILD copy step attached to the wrong target;
`CreationStationInstrument`'s standalone `juce_add_plugin` call bypassed the
wrapper function that had the copy step). All 17 first-party VST plugins now build
into `<exeDir>/Plugins` and are found automatically with zero configured search
paths — confirmed live by the user.

### 6. Suite AI Accounts — fully redesigned, twice-corrected after live review

This is the freshest work and had the most back-and-forth, worth reading closely
if you touch anything AI-account-related.

**Final shape** (after two rounds of live user correction — see commit messages
`46b8f3e` and `cf115c4` in the superproject for the exact "before" that was wrong):

- Named accounts (provider + key + cached model list), created/edited through a
  popup (`shared/UI/SuiteAiAccountDialog`): pick a Provider, enter a Key, press
  **Connect** (not "Refresh" — renamed after review) to call the provider's real
  model-list API and cache the result on the account. **The dialog does not have a
  model picker** — which model to use is chosen at the tool/app level from the
  account's cached list, never baked into the account itself.
- The suite Settings AI tab (`shared/UI/SuiteSettingsPanel`, tab now labeled "Suite
  AI Accounts", not "AI & Routing") shows every account as a real visible list
  (`juce::ListBox` + `AccountListBoxModel`), not a single-selection dropdown you
  page through. **It does not configure any app** — no "Suite Default Account"
  combo, no per-app (Station/Engine/Movie/Live) routing rows. Each app picks its
  own account itself; the suite panel only manages the account list.
- CreationStation's Virtual Engineer (`AiPanel`) picks by account name via
  `setAvailableAccounts`/`onAccountChanged`, shows that account's cached models.
  CreationStation's own app-local Settings panel has **zero** AI provider UI left
  — grepped clean.
- `SuiteAiSettingsStore::refreshAllAccountModelCaches` reconnects every account
  with a key once per app startup (background thread, not polling) so caches stay
  fresh without the user having to manually hit Connect again.
- **Real bug found and fixed via live testing** (not caught by build or review):
  Add/Edit/Remove Account only updated the panel's in-memory state — nothing was
  persisted until the separate "Apply Suite Settings" footer button was clicked.
  User edited an account, hit Connect (125 models fetched), hit Save, and the
  change silently vanished on next Edit. Fixed by having Add/Edit/Remove each call
  `onApplyAiSettingsRequested` immediately (same persist path the footer button
  uses), matching how Connect already does a real, immediate action.

**Progress noted on Creation-Suite#1** (the original BYOK epic — much bigger in
scope than what shipped): only OpenAI's model-list endpoint has actually been
live-tested (125 models, confirmed by the user). The other 7 cloud providers +
Ollama/LM Studio exist as base-URL profiles in `SuiteAiProviderRuntime` but aren't
individually verified against their real APIs. No capability metadata, no
per-provider health-check flow beyond the ad-hoc "Test Account" button. Only
CreationStation consumes the account list today — Engine/Movie/Live don't have an
AI panel wired to it yet.

**Not yet click-tested**: the full Add Account → Connect → pick a model in the
Virtual Engineer → switch accounts flow, after the persistence fix. Build-verified
only (zero errors, single-threaded Debug rebuild after every change).

## Standing rules that bit us this stretch (worth knowing before you touch this code)

- **Single-threaded builds only** — never pass `/m` to MSBuild. The user's machine
  has actually locked up from a multi-core build running while the app was open.
- **MSBuild lives on `D:`, not `C:`**, on this machine: `D:/Program
  Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe`.
  Invoke it directly on a `.vcxproj`/`.sln` via Bash with
  `export MSYS_NO_PATHCONV=1` prefixed, or Git Bash mangles `/t:`/`/nologo`-style
  switches into fake Unix paths.
- **`juce::Array::indexOf`/`exactlyEqual` requires `operator==`** on the element
  type — a real MSVC C2678 caught by the build, not by review, when
  `SuiteAiAccountDialog` tried `providers.indexOf(...)` on a plain struct with no
  `operator==`. Find the index with a manual loop instead.
- **Don't dive into fixing mid-testing-session feedback.** When the user is
  actively running the app and reporting issues (verbally *or* via annotated
  screenshots — both count), collect the feedback and wait for an explicit "done
  testing" / "go fix it" before editing. This got violated twice this stretch;
  see `feedback_dont_fix_during_testing` in the Claude memory store if you have
  access to it, or just ask the user directly if unsure.
- **No premature "done" status.** Build success is not verification. Say plainly
  what's build-verified vs. what still needs a human at the running app.

## Deferred / explicitly not started this stretch

- **Suite Hotkey Registry** — requirements captured on the wiki
  (`wiki/Suite-Hotkey-Registry.md`) and tracked as
  [Creation-Suite#78](https://github.com/wwestlake/Creation-Suite/issues/78), per
  explicit user instruction ("we will plan this out later"). No design or
  implementation work started — leave it alone unless the user raises it again.
- Stage 5/6 of the VFS cleanup (delete `shared/VFS`/FatFs/`SuiteRootProject`/
  `SuiteProjectHandoff`, flip the architecture doc's status section) — paused,
  not abandoned.
- A thin `FoleyPanel` actually hosting the new node-graph editor inside
  CreationStation — the graph/codegen/save-load machinery exists; the panel that
  presents it as a workspace tab does not yet.
- Manual end-to-end verification of the video track feature (real GPU, real video
  file) and the full Suite AI Accounts flow post-persistence-fix.
