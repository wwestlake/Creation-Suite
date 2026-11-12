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

GitHub Requirements CI remains blocked by the pre-existing Station R3 audit:
`Source/Audio/PatchLiveVoice.cpp:193` (`CS_SIGNAL_LAB_RUNTIME`) and
`Source/Language/AudioGraphSelfTest.cpp:46` (`CS_SIGNAL_LAB_NODE_LIBRARY`) in the
unchanged pinned Station source. [PR run 43314020354](https://github.com/wwestlake/Creation-Suite/actions/runs/43314020354)
and [master run 40992946726](https://github.com/wwestlake/Creation-Suite/actions/runs/40992946726)
report the identical failures. These checks remain enforced; the local passing
results do not make the PR merge-ready. The audit defect is outside this
shared-project cleanup.

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
