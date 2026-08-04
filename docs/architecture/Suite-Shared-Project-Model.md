# Suite Shared Project Model

## The vision, stated plainly

A project file is a VFS built around a Project. That's it. There is no import/export between apps and no per-app project silo.

- Any app can open any project.
- Opening a project is opening a project — not importing one, not connecting to a foreign project, just opening it.
- If a project contains assets an app understands, that app can use them.
- Any app can store its own new assets into the project it has open.
- Other apps that have that same project open can see and use those assets too.

Concrete example: load up the DAW (Creation Station) and the video editor (Creation Movie) against the same project. Record a sound in the DAW. That sound is stored in the project. The video editor sees it — instantly, not after an export/import step.

## Why this replaces the "cross-app import" framing

Existing docs (`docs/SUITE_PLATFORM_ARCHITECTURE.md`, and the original board item this replaces, "Phase 4: Real cross-app import via ProjectRegistry") described this as an import/export contract between separate per-app projects: Station has its project, Movie has its project, and an explicit action moves an asset from one into the other. That is the wrong model. There is one project. Apps are viewers/editors onto it, not owners of separate copies that occasionally exchange assets.

## What this rests on (already built)

- `ProjectSession` / `ProjectContainerService` (`shared/AssetSystem`): a project is a real container file with a manifest, asset catalog, and logical-path entries. Nothing here is app-specific.
- `AssetDescriptor` / `AssetKind`: assets are typed and catalogued generically. An app that understands `AssetKind::audio` can use an audio asset regardless of which app wrote it.
- `SuiteAppDomain` on `ProjectManifest`: today this tags a project with an *originating* domain, which is fine as metadata (whose project is this "for" by default) but must not be read as "only this app may open it."
- `SuiteProcessRegistry` (IPC-1): already tracks live suite processes, including `oscPort`/`pipeName` fields reserved for future inter-process transport. Not yet wired to anything — this is the natural place a same-project coordination channel would attach.

## The real open problem this creates

Two separate OS processes (Station.exe, Movie.exe) each running their own `ProjectSession` instance against the *same* container file, potentially both writing, is a genuine multi-writer concurrency problem that does not exist today (today, one process owns one container for its own session lifetime). This needs a real answer, not a hand-wave:

- **Naive risk**: both processes independently read the container, both independently call `commit()`, second writer silently clobbers the first's changes or corrupts the archive.
- **Option A — single-writer arbitration**: one process (first to open, or a dedicated broker) owns the actual file; other apps go through it via IPC (the `oscPort`/`pipeName` transport already reserved on `SuiteProcessRecord` is the natural channel) rather than touching the file directly.
- **Option B — per-asset locking**: apps write to distinct logical paths inside the container almost all the time (Station writes audio stems, Movie writes video renders) — lock at the asset/logical-path level, not the whole container, so concurrent non-conflicting writes never block each other. Matches the "presence + per-object locking" MVP scope already chosen in `Suite-Realtime-Collaboration-Plan.md` for the *remote* multi-user case — this is the same idea applied locally, one machine, multiple apps, no network involved.
- **Live visibility**: an app with the project already open needs to learn "a new asset just landed" without the user closing and reopening — either a file-watch on the container, or a push notification over the same IPC channel used for write arbitration.

This doc doesn't pick a final answer — it exists so the vision itself is written down and the open concurrency question is explicit, not lost. Scoping the actual mechanism (locking granularity, IPC transport, notification path) is the next real design step before implementation.

## Mechanism chosen: a suite-owned background VFS service

Decided 2026-08-03, resolving the "mechanism not yet chosen" status this doc previously ended on.

A single background process — not any app process — owns the VFS/container exclusively. No app ever opens, locks, or writes the container file directly. Every app is a client of this service, not a co-owner of the data.

### Why this beats Option A/B above

Option A (broker) and Option B (per-asset locking) both still let multiple app processes touch the container, just with different arbitration. A dedicated owning service removes the multi-writer problem at the root: there is only ever one writer, by construction, not by cooperation. This also answers the "live visibility" question raised above — the service is the natural place to push "asset changed" notifications from, since it already sees every write.

### Transport: HTTP + WebSocket on localhost

The service exposes a local-only HTTP API (localhost, no external interface) that any suite app on the same machine can call:

- Request/response endpoints for catalog/metadata operations (list, resolve, create, rename, delete) and for reading/writing individual asset bytes — chunked/range-request GET handles bulk asset transfer (large video/audio files) without needing a separate protocol.
- A WebSocket channel on the same server for (a) push notifications — asset changed, lock acquired/released, project opened elsewhere — and (b) genuinely live feeds, e.g. an app streaming a live audio monitor signal through the service rather than polling for it.

Once an app has pulled bytes through the API, it holds them exactly as it always would (decoded audio in memory, a texture in GPU memory, etc.) — the service is only in the data path for the initial fetch, the write-back, and change notifications, not for ongoing playback/editing. This keeps the local server out of any performance-critical path.

### Suite-wide config, not per-app

There is exactly one setting a user makes: the VFS root path (recommended: a large storage device). This is suite-level configuration owned by the background service as the single source of truth, not six copies of the same setting duplicated per app. The pointer to that root (plus other bootstrap config) lives in the normal per-OS app-data location; the actual project/asset data lives wherever the user pointed the VFS.

### Startup flow

On launch, an app's splash screen checks whether the suite is configured (VFS root set, background service reachable) before loading its main window. If not configured, the suite's shared setup/walkthrough flow runs — not an app-local one — to gather what's needed, VFS location foremost among it. Only after that does the app proceed to its normal main screen.

### Cross-app focus / handoff

Because the service is the sole owner, there is no app-to-app lock handoff to design: an app requests what it needs from the service when it gains focus or needs access, and the service — which already tracks who has what open — mediates. This is a simplification of, and likely subsumes, the container-level single-owner exclusive lock work already shipped in Movie's VFS-M4 milestone; formalizing "single owner" as a persistent service process, rather than whichever app currently holds the lock, is the natural next step, not a separate mechanism.

## Relationship to Suite-Realtime-Collaboration-Plan.md

That doc is about multiple *users*, potentially on different machines, editing the same project over a network (LagDaemon.com as broker, WebRTC-style P2P data path). This doc is about multiple *apps*, same machine (today), same project, no network involved. They will likely converge — a project opened locally by two apps and remotely by a second user's machine is the same underlying "who can touch this container and how do writes get coordinated" problem — but the local multi-app case is the more immediate, more foundational one: it has to work before remote collaboration is even worth building.

## Status

Mechanism chosen (see above): a suite-owned background service, sole owner of the VFS, exposed to all apps over a localhost HTTP + WebSocket API. Not yet implemented. Open follow-ups: exact endpoint/message shapes, service lifecycle (who launches it, whether it outlives all apps or exits when the last one closes), and how this formally relates to the existing VFS-M4 per-container lock mechanism already shipped in Movie.

This replaces "Phase 4: Real cross-app import via ProjectRegistry" as the framing for that board item.
