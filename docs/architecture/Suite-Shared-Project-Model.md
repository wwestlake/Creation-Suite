# Suite Shared Project Model

## The vision, stated plainly

A project is a folder in the VFS built around a Project. That's it. There is no import/export between apps and no per-app project silo.

- Any app can open any project.
- Opening a project is opening a project — not importing one, not connecting to a foreign project, just opening it.
- If a project contains assets an app understands, that app can use them.
- Any app can store its own new assets into the project it has open.
- Other apps that have that same project open can see and use those assets too.

Concrete example: load up the DAW (Creation Station) and the video editor (Creation Movie) against the same project. Record a sound in the DAW. That sound is stored in the project. The video editor sees it — instantly, not after an export/import step.

"App" throughout this doc means any suite client with a project open — the six GUI apps, but also any CLI tool, script, or future utility that talks to the VFS service. A project contains assets from whichever app or tool put them there; the service doesn't distinguish between them, and neither should anything built on top of it.

## Why this replaces the "cross-app import" framing

Existing docs (`docs/SUITE_PLATFORM_ARCHITECTURE.md`, and the original board item this replaces, "Phase 4: Real cross-app import via ProjectRegistry") described this as an import/export contract between separate per-app projects: Station has its project, Movie has its project, and an explicit action moves an asset from one into the other. That is the wrong model. There is one project. Apps are viewers/editors onto it, not owners of separate copies that occasionally exchange assets.

## Project storage: a real folder, not a packed container file

A project is a real folder tree in the VFS — plain files and subdirectories on disk under the VFS root — not a single packed container file. This is an explicit, repeated user decision. Every app just stores files in the project it has open; there is no packing/unpacking step and nothing app-specific about the storage shape.

`ProjectSession` / `ProjectContainerService` (`shared/AssetSystem`) today implement a project as a single packed container file (a FatFs-formatted virtual-disk-image, after the `VFS-M1`–`VFS-M4` milestones cut it over from an earlier zip format) — that packed-file mechanism is what needs to change to real folders; it does not reflect the target model and should not be treated as settled. `AssetDescriptor`/`AssetKind` typing and the manifest/catalog concepts are not in question, only the packed-single-file storage underneath them.

`SuiteAppDomain` on `ProjectManifest` today tags a project with an *originating* domain, which is fine as metadata (whose project is this "for" by default) but must not be read as "only this app may open it."

**Status of this correction**: documented now (2026-08-11) after being stated more than once and not landing in this doc previously — see the root `AGENTS.md`'s Storage Boundary Rule and "No false backward-compat" precedent (memory: `feedback-no-false-backward-compat`) for the same underlying pattern: don't preserve an implementation choice just because it already exists and works, in a suite that is still under construction. The actual migration off the packed-container format to plain folders has not been implemented yet — tracked as follow-up work.

## Mechanism: a suite-owned background VFS service, sole owner of the entire VFS

Decided 2026-08-03; project storage explicitly folded into it above.

A single background process — `CreationSuiteVfsService`, not any app process — owns the entire VFS exclusively: settings entries and project folders alike, the whole tree, one owner, by construction. No app ever touches VFS files directly, on any part of the tree, ever. Every app is a client of this service. There is no handoff model and no concurrent ownership between apps: every app just talks to the service, for every read/write, whenever it needs to, regardless of focus state — it stores/reads files in whatever project it has open, through the service, same as any other VFS access.

This removes the multi-writer problem at the root: there is only ever one writer, by construction, not by cooperation between apps. It also gives a natural place to push "asset changed" notifications from, since the service already sees every write.

VFS-M4's per-container app-held exclusive lock mechanism (shipped in Movie) does not carry forward under this model — it assumed an app itself could hold a lock on a container file, and under the service-owns-everything model no app ever holds a lock on anything. Not an open question to resolve; a superseded mechanism to retire, not preserve as a parallel path.

### Transport: HTTP + WebSocket on localhost

The service exposes a local-only HTTP API (localhost, no external interface) that any suite app on the same machine can call:

- Request/response endpoints for catalog/metadata operations (list, resolve, create, rename, delete) and for reading/writing individual asset bytes — chunked/range-request GET handles bulk asset transfer (large video/audio files) without needing a separate protocol.
- A WebSocket channel on the same server for (a) push notifications — asset changed, project opened elsewhere — and (b) genuinely live feeds, e.g. an app streaming a live audio monitor signal through the service rather than polling for it.

Once an app has pulled bytes through the API, it holds them exactly as it always would (decoded audio in memory, a texture in GPU memory, etc.) — the service is only in the data path for the initial fetch, the write-back, and change notifications, not for ongoing playback/editing. This keeps the local server out of any performance-critical path.

### Suite-wide config, not per-app

There is exactly one setting a user makes: the VFS root path (recommended: a large storage device). This is suite-level configuration owned by the background service as the single source of truth, not six copies of the same setting duplicated per app. The pointer to that root (plus other bootstrap config) lives in the normal per-OS app-data location; the actual project/asset data lives wherever the user pointed the VFS.

### Startup flow

On launch, an app's splash screen checks whether the suite is configured (VFS root set, background service reachable) before loading its main window. If not configured, the suite's shared setup/walkthrough flow runs — not an app-local one — to gather what's needed, VFS location foremost among it. Only after that does the app proceed to its normal main screen.

## Relationship to Suite-Realtime-Collaboration-Plan.md

That doc is about multiple *users*, potentially on different machines, editing the same project over a network (LagDaemon.com as broker, WebRTC-style P2P data path). This doc is about multiple *apps*, same machine, same project, no network involved. They will likely converge — a project opened locally by two apps and remotely by a second user's machine is the same underlying "how do writes get coordinated" problem — but the local multi-app case is the more immediate, more foundational one: it has to work before remote collaboration is even worth building.

## Status

Mechanism decided and partially implemented: `services/VfsService` is real, built, and functional today for suite-level entries (settings, AI config — confirmed working end to end in the 2026-08-11 settings-storage migration). Not yet implemented: migrating project storage off `ProjectContainerService`'s packed-container format onto real folders under the same service-owned tree. Open follow-ups: exact endpoint/message shapes for project-folder operations, service lifecycle (who launches it, whether it outlives all apps or exits when the last one closes).

This replaces "Phase 4: Real cross-app import via ProjectRegistry" as the framing for that board item.
