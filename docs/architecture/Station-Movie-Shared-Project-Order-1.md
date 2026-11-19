# Station / Movie shared projects — Work Order 1

Authorized by Bill on November 12, 2026. Tracking: Suite #213, Station #93,
Movie #49. Order 2 is not part of this work. PRs require review and authorization
before merge.

## Authority and cause

`Suite-Shared-Project-Model.md` is the current project authority, as referenced
by the repository's `AGENTS.md`: any app opens the same Suite project, and uses
its compatible assets without a cross-app export/import operation. The current
ProjectSession, project manifest/catalog and VFS service implement that model.

Movie nevertheless required `Project/movie-state.json`, exposed only its saved
sequence's media records, and accepted only `Media/` source paths. Its import
and recording paths did not publish those references to the shared catalog.
Station opened the shared project but returned from session loading when
`session.xml` was absent, leaving the preceding arrangement in memory. In
addition, manifest saves replaced an app's entire cached catalog, allowing a
stale session to erase another app's catalog additions.

## Decisions

- Keep the existing Suite project identity, VFS service and asset catalog.
  No new project container, interchange format or cross-app copying step.
- Station continues to own `session.xml`; Movie continues to own
  `Project/movie-state.json`. Opening a project with no state for the current
  app starts a fresh native workspace. An unreadable or malformed existing
  state is an error, rather than permission to replace it with a blank edit.
- Movie's media records are resolved references and decoder metadata. The Suite
  catalog supplies stable asset/version IDs and VFS paths. New external imports
  and finalized recordings are registered in that catalog. Previously stored
  Movie media are registered by reference on save, without copying their bytes.
  A `catalogued` provenance flag prevents an old edit from resurrecting a
  subsequently removed catalog entry.
- Multiple versions of one asset may coexist in Movie. Each placement persists
  its exact version; source selection, placement and decoder cache resolution
  honor that version. Undo retains independently discovered project media.
- Refresh reads project metadata and probes new compatible media through VFS
  streams on Movie's background worker. Unsupported/unreadable new media produce
  warnings. Station's existing asset enumeration refreshes its manifest. No
  shared playhead, transport synchronization or timeline conversion is added.
- Manifest commit uses a three-way merge of the session's baseline, its edits
  and the current service manifest. Disjoint catalog/metadata changes survive;
  competing edits to the same field/version fail visibly and retain local edits.
  The existing HTTP manifest endpoint requires `If-Match`, checks the revision
  under the service lock and assigns the next revision. Bounded retries handle
  a second writer between read and commit. This extends the established VFS API;
  it is not an interchange protocol.
- A capability header prevents rebuilt clients from attempting unsafe catalog
  saves through an older service. The updated service is a deployment prerequisite.
  The service rejects unguarded older writers with HTTP 428; the unsafe client
  replacement method is removed, and the existing Services smoke caller uses
  revision-checked writes. Update writer binaries along with the service.

## Validation

All final single-worker Debug builds pass: Station, Movie and the VFS service.

| Check | Observed result |
| --- | --- |
| Movie document regression | Pass: migration, stable tracks, media/clip serialization, effects, automation, transitions, undo/redo, shared paths and exact versions |
| Station regression | LinkedClipsSmoke and SignalClipRenderSmoke: all passed, zero failures |
| Shared manifest merge | Pass: independent additions, stale saves, removal/update conflicts, atomic rejection, metadata property ordering, version and project identity |
| Live project sessions | Pass: peer catalog preservation, conditional revision rejection, missing-revision rejection, optional state, native document byte preservation and repeated saves |
| Station mounted preflight | Pass: a Movie-origin project with no Station session gets a fresh workspace; opaque Movie bytes remain untouched |
| Acceptance A/B/C | All six actual-app groups pass, including real Station WAV/MP4 import, direct Movie opening, Movie-origin opening in Station and three repeated cross-app save cycles |
| Media/version rendering | Decoded export has visible video and non-silent audio. The original 440 Hz component exceeds the new 880 Hz component by 8,895,870:1 after refresh; peak 0.08837890625, pixel range 255 |
| Service bypass protection | Unguarded PUT returns HTTP 428 without changing the manifest revision |
| Cleanup | Every owned app process exited; the owned test service was stopped and the final process inventory was empty |

Final local evidence is under
`apps/CreationMovie/build/order1-check-f3679d93/`, including `results.json`, native
Station results, original native state snapshots and the decoded-test export.
Build and console logs use `order1-*-verified-*`, `order1-vfs-final-build.log` and
`order1-acceptance-final.log` in the existing app build directories. Existing
compiler warnings remain.

The mounted check is `scripts/Check-StationMovieSharedProject.py`. It uses real
Station import/placement/save code and Movie's authenticated application commands,
unique disposable projects, real WAV/MP4 inputs and a decoded FFV1/PCM export.
It checks both native state entries and repeated cross-app persistence. This is
application automation, not a claim of native mouse/keyboard testing, subjective
listening, a broad codec matrix or crash recovery.

The caller owns the test VFS service lifetime. The script closes its app
processes and removes only its own exact project IDs. All launched test processes,
including the test service, must be stopped after validation.

## Review and remaining boundaries

Shared changes are in AssetSystem (sessions, optional state, manifest merge and
service client), the existing VFS manifest route, the Services smoke caller and
focused tests. Station changes are in project initialization/open/save, catalog
enumeration and a Debug acceptance harness. Movie changes are in catalog/media
resolution, import/save registration, exact version serialization/decoding and
its existing media panel's Refresh action.

The Suite shared-platform PR is a prerequisite for the app PRs. Writer binaries
and the VFS service must be updated together. Existing app gitlinks stay on their
merged pins until the app PRs actually merge; that later pointer update requires
its own review. Nothing in this order authorizes a merge or Order 2.

The original GitHub Requirements failures came from Station pin `717a5f0`:
`Source/Audio/PatchLiveVoice.cpp:193` (`CS_SIGNAL_LAB_RUNTIME`) and
`Source/Language/AudioGraphSelfTest.cpp:46` (`CS_SIGNAL_LAB_NODE_LIBRARY`).
[PR run 43314691246](https://github.com/wwestlake/Creation-Suite/actions/runs/43314691246)
records that failure. A later local pass with a newer Station checkout did not
clear the pinned PR. The November 18 baseline correction below replaces that
pin with reviewed canonical Station master and checks the actual PR/submodule
state. Requirements remain enforced; the updated PR must pass its own CI.

Catalog refresh while apps stay open is explicit. Automatic event-driven media
refresh, shared transport, scoring/Foley work, new effects/plugin features,
proxy/HDR work and same-app multi-user editing are outside this order. The
acceptance run does not claim manual pointer interaction or subjective listening.

## Published review changes

| Repository | Pull request | Implementation commit |
| --- | --- | --- |
| Suite shared platform | [#214](https://github.com/wwestlake/Creation-Suite/pull/214) | `5dd3223` |
| Station | [#94](https://github.com/wwestlake/CreationStation/pull/94) | `5767b7b` |
| Movie | [#50](https://github.com/wwestlake/Creation-Movie/pull/50) | `c82d2fb` |

All are submitted for review without merging. The Suite PR is the app PRs'
shared-code prerequisite. No unmerged application gitlink is included.

## Station baseline correction, November 18

Authorized by [the corrective work order](https://github.com/wwestlake/Creation-Suite/issues/215#issuecomment-6992370794).
The former pin `717a5f0` and Station master are divergent: their common ancestor
is `5aafc83`. The old side includes `40bdb93` (a development merge), `79b4a50`
(the unmerged beta-version bump), and policy backports `9f1b407` / `717a5f0`.
This correction deliberately follows accepted master rather than retaining that
development line or importing local casting work.

Master `757a8c5` contains merged Station PRs #79–#90 and a shared-workspace policy
commit. Its 47-file delta is inventoried below. The accepted changes are:

| Accepted PRs | Effect relative to the old pin |
| --- | --- |
| #79–#80 | Use the Suite FRust host; embed built-in Signal Lab FRust and load from memory; move the path-based self-test out of app Source. This removes the R3 violations. |
| #81–#84 | Assistant input/height fixes, Station script host/API, Producer/Engineer tools, listening analysis and help cards, project conversation storage/Chats UI. |
| #85–#87, #89–#90 | Install the VFS service with Station; verify MSI contents; fix installer components/shortcuts and chosen suite-folder layout. |
| #88 | Require pinned GPLv3 ASIO SDK 2.3.4 in every build and enforce it in release checks. |
| `757a8c5` | Record the shared-workspace policy. |

The added shared targets (AiCore, AudioAnalysis, AssistantStore, AiProviders and
AiFrustTools) already exist in this Suite PR's shared baseline. Shared/service
source at the previously tested Suite `85bf0ec` is identical to PR head
`38a1e5a`; the canonical Station Debug/offline checks therefore exercised these
same library sources. No dependency installation or LLVM change is required.
The canonical version remains `0.5.1`; the unmerged beta-version branch is not
silently retained.

Executive Director review then merged Station #97 as canonical master
`20d3c41b7c2156c022fd2509d8fc03d21fcd71a0`. Its delta from `757a8c5` is only
README/quarantine documentation (`68d4416`, `dd5c7c6`, merge `20d3c41`); runtime,
build and test sources are identical. This exact merged master is the new Suite
pin. Casting, Station #94 and the preservation branch are excluded. The private
casting archive remains intact. Movie/Android/FRust and all other pins are
unchanged. This does not merge the app compatibility PRs or authorize transport,
capture, MIDI, Android splitting or hardware work.


### Complete master-side commit inventory through 757a8c5

```text
cbeb478 Merge pull request #79 from wwestlake/claude/frust-host
a47d3e0 feat: Signal Lab FRust is embedded in the app and loaded from memory; release checks the exe ships no libraries
04ec95b Merge pull request #80 from wwestlake/claude/rules-checks
9793726 feat: Virtual Engineer message box starts one line and grows; arrow send button inside it; Enter-sends option
494b3a8 Merge pull request #81 from wwestlake/claude/ai-prompt-box
77c8ade fix: message box height comes from measuring the text, so it shrinks as well as grows; register the shared AI framework; Station's script API (not yet wired)
7f780af Merge pull request #82 from wwestlake/claude/development
ce0a837 feat: Station's host for AI scripts over the real project, and a smoke test of Station's script API
64c1f8d feat: the Virtual Engineer works as a coder - writes and runs FRust against Station through the agent engine
2f27360 feat: the Virtual Engineer is given a FRust guide; failed runs show the real errors; tests for the guide
f2596d3 feat: Producer and Engineer roles; listening tools; full track control and automation for scripts; music and FRust help cards
998b9cc Merge pull request #83 from wwestlake/claude/development
ebf152a feat: the assistant's conversations are recorded in the project and managed from a Chats window
cffaccd Merge pull request #84 from wwestlake/claude/development
dce1491 feat: the release installs the storage service with Station, and refuses to publish without it
8925d78 Merge pull request #85 from wwestlake/claude/development
cec725d fix: installer - one file per component, and per-machine key paths for the shortcuts
8b4bf82 Merge pull request #86 from wwestlake/claude/development
da2d8bd fix: installer - per-user key paths for the shortcuts, and skip ICE57
90a17f2 Merge pull request #87 from wwestlake/claude/development
be80582 build: ASIO is required in every build (GPLv3 ASIO SDK 2.3.4, pinned), and the release checks for it
9f5a270 docs: AGENTS.md - ASIO is required and must never be disabled
6fd3ac3 Merge pull request #88 from wwestlake/claude/development
156a294 fix: the release's installer check unpacks the MSI instead of querying it through COM
691b22f Merge pull request #89 from wwestlake/claude/installer-verify-fix
2130037 installer: everything goes into the suite folder the user chooses (Station and Services under it)
5239cd5 Merge pull request #90 from wwestlake/claude/suite-folder-layout
757a8c5 docs: use the shared Suite workspace for all LLMs
```

### Complete changed-file inventory through 757a8c5

```text
M	.github/workflows/release.yml
M	CMakeLists.txt
A	Source/Agent/StationAgentApi.frust
A	Source/Agent/StationAgentHost.h
A	Source/Agent/StationAgentHostImpl.cpp
A	Source/Agent/StationAssistant.cpp
A	Source/Agent/StationAssistant.h
A	Source/Agent/StationConversations.cpp
A	Source/Agent/StationScriptApi.cpp
A	Source/Agent/StationScriptApi.h
A	Source/Agent/StationScriptGuide.md
A	Source/Agent/VfsAssistantFiles.h
M	Source/Audio/PatchLiveVoice.cpp
M	Source/MainComponent.cpp
M	Source/MainComponent.h
M	Source/Views/AiPanel.cpp
M	Source/Views/AiPanel.h
A	Source/Views/ConversationManager.cpp
A	Source/Views/ConversationManager.h
A	Source/Views/PromptInputParts.h
A	help/tools/make_conversation_topics.py
A	help/tools/make_frust_topics.py
A	help/tools/make_music_topics.py
A	help/tools/make_theory_topics.py
A	help/topics/en-US/djehuti.station.assistant-writes-scripts.json
A	help/topics/en-US/djehuti.station.conversations-managing.json
A	help/topics/en-US/djehuti.station.conversations-saved-securely.json
A	help/topics/en-US/djehuti.station.frust-compile-errors.json
A	help/topics/en-US/djehuti.station.frust-language.json
A	help/topics/en-US/djehuti.station.frust-station-api.json
A	help/topics/en-US/djehuti.station.music-judging-intent.json
A	help/topics/en-US/djehuti.station.music-pitch-interpretation.json
A	help/topics/en-US/djehuti.station.music-rhythm-interpretation.json
A	help/topics/en-US/djehuti.station.producer-tools-guide.json
A	help/topics/en-US/djehuti.station.theory-chords-harmony.json
A	help/topics/en-US/djehuti.station.theory-form-and-vocabulary.json
A	help/topics/en-US/djehuti.station.theory-keys-circle-of-fifths.json
A	help/topics/en-US/djehuti.station.theory-notes-intervals-scales.json
A	help/topics/en-US/djehuti.station.theory-rhythm-meter.json
A	help/topics/en-US/djehuti.station.theory-tuning-intonation.json
A	installer/DjehutiStation.wxs
A	installer/build-msi.ps1
R100	Source/Language/AudioGraphSelfTest.cpp	tests/AudioGraphSelfTest.cpp
R100	Source/Language/AudioGraphSelfTest.h	tests/AudioGraphSelfTest.h
M	tests/StationFrustIntegrationSmoke.cpp
A	tests/StationScriptApiSmoke.cpp
A	tests/StationScriptGuideSmoke.cpp
```
