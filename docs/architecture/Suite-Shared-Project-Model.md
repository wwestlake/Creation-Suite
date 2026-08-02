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

## Relationship to Suite-Realtime-Collaboration-Plan.md

That doc is about multiple *users*, potentially on different machines, editing the same project over a network (LagDaemon.com as broker, WebRTC-style P2P data path). This doc is about multiple *apps*, same machine (today), same project, no network involved. They will likely converge — a project opened locally by two apps and remotely by a second user's machine is the same underlying "who can touch this container and how do writes get coordinated" problem — but the local multi-app case is the more immediate, more foundational one: it has to work before remote collaboration is even worth building.

## Status

Vision captured, mechanism not yet chosen. This replaces "Phase 4: Real cross-app import via ProjectRegistry" as the framing for that board item.
