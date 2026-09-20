# Suite VFS: One Container, Nothing Else On Disk

Status: plan, agreed in conversation 2026-09-20. Phase 1 starting.

## The rule

The suite writes exactly this to the operating system's disk, and nothing else:

1. **The root pointer**: `suite-settings.json` in the user's roaming app data (`...\Roaming\Djehuti-Suite\`). One field, `suiteVfsRoot`.
2. **The VFS root folder**, which holds only:
   - `vfs.bin` - the container. A single sparse file holding an exFAT (FatFs) volume. Every project, asset, setting, layout, mapping, log and cache lives inside it.
   - `VfsHeartbeat.json` - one small file written by the VFS service: its process id, a random free port chosen at start-up, and a timestamp. Apps read it to find the service. A stale file (old timestamp, dead process id) is ignored.
3. **Explicit exports** to a folder the user picks, when the user clicks Export.

No subfolders in the root. No files written by apps. No copying an asset out of the VFS to a real file. No OS temp folder. The VFS service is the only program that opens `vfs.bin`, and it closes itself after 60 minutes with no requests, and any app that needs it starts it again.

Only the service writes to `vfs.bin`. Apps talk to the service over local HTTP.

## Why this document exists

The single-container design was built on 2026-08-02 (`SuiteVolume`, FatFs/exFAT, verified with >4 GB files). On 2026-08-12 commit `728d35f` replaced it with plain folders on disk (`VfsProjectStore`) and rewrote the docs to say "projects are folders". Everything since - the `Cache`, `Logs`, `Scratch`, `processes`, `suite` and `Project Containers` folders, the copy-out of assets to real files, the leases that were never released - follows from that. The container code in `shared/VFS` still builds and is tested; the service just does not use it.

The old container extension `.csproj` is also retired: it is a Microsoft C# project extension. The container is `vfs.bin`.

## Decisions

| Decision | Value | State |
| --- | --- | --- |
| Container file name | `vfs.bin` | agreed |
| Root folder contents | `vfs.bin` + `VfsHeartbeat.json`, no folders | agreed |
| App heartbeat files | dropped; apps write no files | agreed |
| Service discovery | one `VfsHeartbeat.json`, random port | agreed |
| Service shutdown | closes itself after 60 minutes of inactivity (no requests); apps start it again when they need it (user, 2026-09-20) | agreed |
| Container size limit | 1 TB, sparse; a configuration value that can be raised later | default in use (never objected) |
| Existing folder-based data | not migrated; it is test data and may be deleted once the new service is running (user, 2026-09-20) | agreed |

## Phases

### Phase 1 - Service back on the container
- The service creates `vfs.bin` on first run and opens it for the life of the process (FatFs allows one mounter).
- Rewrite `VfsProjectStore` to keep projects, entries and manifests inside the volume. The HTTP endpoints (`/project/...`, chunked upload, ranged download) keep their shapes, so apps do not change.
- Replace `SuiteProcessRegistry` with the single `VfsHeartbeat.json` (random port). Remove app registration (`RegisterSelf`). The service's idle shutdown changes from "no other app is running" to "no requests for 60 minutes".
- Acceptance: existing service smoke tests pass against the container; a fresh root contains only `vfs.bin` and `VfsHeartbeat.json`.

### Phase 2 - Move everything else inside
- `suite/` settings, layouts and mappings: already reached through the service, so they simply live in the container.
- Logs: entries inside the container, written through the service.
- Scratch/temp data: memory, or temporary entries inside the container.
- Remove `getScratchDirectory`, `getLogsDirectory`, the `Cache`/`Scratch`/`Logs`/`processes` folders and the folder-creating code behind them.
- Check what `CreationEngine`'s `DjehutiImportWatcher` needs from the app registry before removing it.

### Phase 3 - Stop copying assets out
- Asset details come only from the manifest (fixes the 20-second click: today `describeProjectAsset` copies the whole file out just to read its format).
- Imports record details (length, sample rate, channels, size) in the manifest at import time.
- Audio: readers read a stream backed by ranged reads from the service.
- Video: a custom byte stream feeds the Windows decoder; the video's sound is extracted in memory and stored back through the service.
- Delete the materialize/lease mechanism (`AssetMaterializer`, `MaterializedAssetLease`, `getMaterializedFilesDirectory`).
- Exports write only to the folder the user picked.

### Phase 4 - Prove it and lock it
- Run Station under Process Monitor through a full session (import, play, click assets, record, render, close). Only the pointer, `vfs.bin` and `VfsHeartbeat.json` may be written. Record the result here.
- A build-time test that fails on banned calls (OS temp folders, materialize, writing files outside the allowed list).
- The rule in `AGENTS.md` (Storage Boundary Rule) is updated to match this document.

## Order
Phase 1 is the largest and riskiest; Phase 3 video is the hardest part. Phase 3's "details from the manifest" step is small and can ship first as a stop-gap only if the user agrees, because it stops new copies immediately.
