# Creation Remote Protocol

Wire contract between the Android client ([Creation-Remote-Android](https://github.com/wwestlake/Creation-Remote-Android), CR-M5) and the Suite Remote Receiver (`apps/CreationRemoteReceiver`, CR-M1), brokered by lagdaemon.com (CR-M2/M4, lives in the `djehuti` repo). Origin: [Epic #65](https://github.com/wwestlake/Creation-Suite/issues/65), reconciled 2026-08-22 — see that issue's session-update comment for the full decision history.

## 1. Hard rule: no captured media ever touches the server

lagdaemon.com's entire role is **authentication and WebRTC signaling**. It never relays, stores, or stages any captured photo/video/audio, under any circumstance — no TURN relay, no S3 fallback, no queue-on-server. If a direct connection to the paired receiver can't be established, the file stays on the phone (local WorkManager-backed queue) and the app retries later. This is a correction of the epic's original CR-M7/relay-based design, made once native (CameraX + WorkManager) removed the reason that design existed — see the epic issue for why.

## 2. Device pairing

- Receiver app displays a short-lived pairing code/QR in its settings.
- Phone scans it, submits it to lagdaemon.com along with the account's auth token.
- Server links the two under the account, issues both sides a long-lived device credential.
- Devices are listed per-account (name, type, last-seen), revocable from the website and/or receiver app. An account can have multiple paired machines; the phone keeps a switchable list with one marked active.

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
