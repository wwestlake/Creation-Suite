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

## Project storage: inside the one container

Every project lives inside the suite's single container, `vfs.bin`, in the VFS root, reached only through the VFS service (`docs/architecture/Suite-VFS-Single-Container-Plan.md`). `ProjectContainerService` (`shared/AssetSystem`) creates and opens projects through the service, by project id; no caller ever sees a real path. Every app just stores files in the project it has open; nothing about the storage shape is app-specific. `AssetDescriptor`/`AssetKind` typing and the manifest/catalog concepts sit on top of that.

`SuiteAppDomain` on `ProjectManifest` today tags a project with an *originating* domain, which is fine as metadata (whose project is this "for" by default) but must not be read as "only this app may open it."

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

There is exactly one setting a user makes outside the suite: the VFS root path (recommended: a large storage device). Everything else lives in the VFS, once, for every app.

**On the OS, exactly one file:** `%APPDATA%\Djehuti-Suite\suite-settings.json`, which holds only the VFS root pointer. Nothing else is written there, by any app or shared library; even the service's process registry (how apps find or start the service) lives inside the VFS root. One place for everything is the point: per-app folders in AppData would scatter copies of settings and keys across the machine, and nothing would know which is current.

**Everything else is a suite entry in the VFS**, read and written through the service with `SuiteVfsJsonStore` (`shared/Services`). The suite's own entries:

- `ai-settings.json`: the AI accounts, their keys and models, and which account each app uses. A key is entered once, in the suite's settings; every app and tool that uses AI reads it from here. No app asks for or stores a key of its own.
- `suite-ai-health.json`: each AI account's recent health (rate limits, failures, cooldowns).
- `suite-ai-diagnostics.json`: the AI diagnostics events.
- `suite-activity-log.json`: the suite activity log.

Each app's own settings are entries too (`station-settings.json`, `engine-settings.json`, ...), never files beside the app.

Tests never touch these: `SuiteVfsJsonStore::setScopeForTesting` sends every entry a test saves to `tests/<scope>/` in the VFS, and `removeScopeForTesting` removes them afterwards.

### Startup flow

On launch, an app's splash screen checks whether the suite is configured (VFS root set, background service reachable) before loading its main window. If not configured, the suite's shared setup/walkthrough flow runs — not an app-local one — to gather what's needed, VFS location foremost among it. Only after that does the app proceed to its normal main screen.

## Shared active-project context

The shared shell owns the active `ProjectSession` for each running app. The
header's project control opens the common Project Manager. When a project is
selected, the shell opens its session, updates the header to `Project: <name>`,
and passes that same session to the app. A project name in the header is
therefore the currently selected VFS project, not a cosmetic label or an
app-local copy of project state.

Applications may remember the last successfully opened project and restore it
at startup. That is a convenience only: the shared project session remains the
authoritative current context.

### Creation Developer workspace

Creation Developer uses the active Suite project as the parent container for
FRust Suite-plugin development. Its Frate VFS Terminal creates plugin pods in
the project-owned FRust working area instead of writing to an arbitrary local
directory. The current scaffold contains pod metadata and FRust source under
the project's plugin-pod workspace, with generated material kept separately.

The terminal roles intentionally remain separate:

- The OS Terminal runs PowerShell against the host operating system.
- The FRust Terminal evaluates code at the `fr->` prompt.
- The Frate VFS Terminal manages Suite plugin-pod work at the `frate>` prompt.

This separation prevents a command intended to operate on a Suite VFS project
from accidentally behaving like a host-filesystem package command.

## Relationship to Suite-Realtime-Collaboration-Plan.md

That doc is about multiple *users*, potentially on different machines, editing the same project over a network (LagDaemon.com as broker, WebRTC-style P2P data path). This doc is about multiple *apps*, same machine, same project, no network involved. They will likely converge — a project opened locally by two apps and remotely by a second user's machine is the same underlying "how do writes get coordinated" problem — but the local multi-app case is the more immediate, more foundational one: it has to work before remote collaboration is even worth building.

## Status

`services/VfsService` is real and is the only owner of the VFS: suite entries (above) and projects both live inside `vfs.bin` and go through it. The first app that needs it starts it (`SuiteVfsServiceClient::discover`), and it closes itself after 60 minutes without requests. What is still unfinished is tracked in `docs/architecture/Suite-VFS-Single-Container-Plan.md`.

This replaces "Phase 4: Real cross-app import via ProjectRegistry" as the framing for that board item.
