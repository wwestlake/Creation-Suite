# Creation Remote Protocol

Wire contract between the Android client ([Creation-Remote-Android](https://github.com/wwestlake/Creation-Remote-Android), CR-M5) and the Suite Remote Receiver (`apps/CreationRemoteReceiver`, CR-M1), brokered by lagdaemon.com (CR-M2/M4, lives in the `djehuti` repo, `src/Djehuti.Api/RemotePairingRepository.fs`). Origin: [Epic #65](https://github.com/wwestlake/Creation-Suite/issues/65), reconciled 2026-08-22 — see that issue's session-update comment for the full decision history.

**Implementation status (2026-08-23):** §2 (pairing) is real and built on both ends — server endpoints, receiver check-in/QR display, phone sign-in/scan/approve. §3 (project listing), §4 (signaling), §5/§6 (asset transfer/deposit) are still design-only, not implemented.

## 1. Hard rule: no captured media ever touches the server

lagdaemon.com's entire role is **authentication and WebRTC signaling**. It never relays, stores, or stages any captured photo/video/audio, under any circumstance — no TURN relay, no S3 fallback, no queue-on-server. If a direct connection to the paired receiver can't be established, the file stays on the phone (local WorkManager-backed queue) and the app retries later. This is a correction of the epic's original CR-M7/relay-based design, made once native (CameraX + WorkManager) removed the reason that design existed — see the epic issue for why.

## 2. Device pairing — implemented

Schema: `remote_host_sessions` / `remote_pairings` / `remote_connection_grants` (migrations 83/84 in `Database.fs`) — backfilled from an earlier, paused remote-control effort (#22) and reused as-is, not duplicated. Auth on every route below is the existing desktop OAuth2/PKCE flow's Bearer/cookie check (`tryGetAuthClaims`) — no new auth mechanism; receiver and phone both sign in as the same account via `SuiteDesktopAuthSession`'s loopback-redirect flow (the Android client reuses this exact flow too — see `Creation-Remote-Android`'s `auth/AuthSession.kt`).

- `POST /api/remote/host-sessions/check-in` — receiver announces/refreshes presence. Body: `{ productSlug, appId, appVersion, deviceId, deviceName, agentAvailable, controlPanelAvailable, capabilities: string[] }`. Upserted on `(user, productSlug, deviceId)`. Returns `{ hostSessionId, presenceState }`. Receiver calls this every 30s as a heartbeat.
- `POST /api/remote/pairings` — receiver mints a pairing code. Body: `{ hostSessionId }`. Returns `{ pairingId, pairingCode, expiresAt }` (10-minute expiry). Receiver renders `pairingCode` as a QR with payload `creationremote://pair?code=<pairingCode>`.
- `GET /api/remote/pairings/{id}/status` — receiver polls while the QR is on screen (no push/WebSocket relay yet — see §4). Returns `{ status: "pending" | "approved" }`.
- `POST /api/remote/pairings/{code}/approve` — phone submits the scanned code. Body: `{ remoteDeviceName, remoteDeviceType }`. Approving account must match the pairing's account. Returns `{ grantToken, hostSessionId, expiresAt }` (30-day grant).
- `GET /api/remote/devices` — phone's switchable paired-device list: `[{ hostSessionId, grantToken, deviceName, productSlug, presenceState, lastHeartbeatAt, grantExpiresAt }]`.
- `DELETE /api/remote/devices/{grantToken}` — revoke a pairing (website and/or phone).

Pairing codes are 8 characters from a Crockford-ish alphabet (ambiguous characters removed) for the manual-entry fallback if QR scanning fails.

## 3. Project listing

- Phone queries the currently-online active receiver's available projects through the server: `GET` (or equivalent) project-list call, authenticated to the account + device pair.
- Receiver sources this from its local `ProjectRegistry::discoverProjects` (existing suite infrastructure, not new plumbing) and keeps it current with the server via its check-in connection.
- Response: array of `{ projectId, displayName }`. The phone shows names; every downstream reference uses `projectId` (avoids name-collision ambiguity).

## 4. WebRTC signaling

- Offer/answer/ICE candidate exchange between phone and the active paired receiver, relayed through lagdaemon.com, authenticated per-account/device-pair.
- STUN for NAT traversal. **No TURN.** A signaling/negotiation failure is not retried via a relay fallback — it's treated the same as "receiver unreachable," and the phone queues locally per §1.
- Once a direct `RTCDataChannel` (or equivalent) is established, all further traffic in this contract (§5) flows over it directly — the server is no longer involved for that asset.

## 5. Asset payload framing

Sent over the direct channel established in §4, phone → receiver.

**Single-shot assets** (photos, short audio clips):
```
{
  "projectId": "...",
  "sessionText": "...",       // user-entered session metadata, used in filename/description
  "kind": "photo" | "audio",
  "capturedAtUtc": "...",
  "sha256": "...",
  "byteLength": N
}
```
followed by the raw binary. Receiver verifies the checksum before writing.

**Chunked assets** (video, long-form audio) — same header shape plus:
```
{
  "totalChunks": N,
  "chunkIndex": i,
  "chunkSha256": "...",
  "isFinalChunk": bool
}
```
per chunk. Receiver stages chunks and performs an atomic move into the project's VFS only once every chunk has arrived and verified — a partially-received asset must never appear as a complete one.

## 6. Receiver-side deposit

On a fully-verified asset (single-shot or fully reassembled chunked): receiver opens the target project by `projectId`, writes via the existing `writeEntry` + `upsertAssetDescriptor` + `commit` path, closes. Brief, self-contained transaction — `SuiteProjectHandoff` already covers another app wanting the project mid-receive. No standing lock is held between assets.

## 7. Explicitly open, not decided here

- Exact transport-layer message envelope/serialization (JSON vs. a binary framing) for §4/§5 — pick when CR-M2/M4 implementation starts.
- Auth token refresh/expiry policy shared with Hermes' publish-auth question (see `shared/CEL/docs/HERMES_PACKAGE_MANAGER_SPEC.md`) — worth deciding once, not twice.
- Whether per-shot metadata beyond `sessionText` (geolocation, device info) is in scope for v1.
