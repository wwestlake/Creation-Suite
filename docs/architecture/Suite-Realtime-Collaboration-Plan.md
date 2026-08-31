# Suite Real-Time Collaboration (DCC-over-the-internet)

Quick-capture spec, written 2026-08-02 — the user flagged this as
something "we keep dancing around" without ever writing down, so this
exists to stop that. Distinct from Hermes (`FRust/docs/
HERMES_PACKAGE_MANAGER_SPEC.md`) and the FRust v2 language work — this is
a suite-wide feature (multi-user live collaboration inside Engine,
Movie, Station, Live), not a FRust-specific one, even though it will
eventually need to interoperate with FRust modules/assets like everything
else in the Suite.

Tracking: not yet filed as GitHub issues. This is a much larger,
harder-to-scope feature than the other two specs written tonight —
review before breaking into milestones, don't file issues off a
first-pass capture like this one.

## 1. Core model: LagDaemon.com brokers, peers talk directly

Confirmed directly: LagDaemon.com is the broker/rendezvous point, not
the data path. The pattern is the standard WebRTC-style signaling
architecture:
- LagDaemon.com handles **session/room management** (who's in this
  collaboration session, invite/join), **authentication**, and
  **signaling** (peers exchange connection offers/answers and ICE
  candidates through it to find each other and negotiate a direct
  connection).
- Once negotiated, actual project data — edits, live cursor/selection
  state, asset updates — flows **peer-to-peer, directly between
  clients**, not through LagDaemon.com. Keeps server bandwidth/cost low,
  keeps collaboration latency to the direct peer path, and keeps project
  content off the central server by default.
- Real P2P over the internet needs NAT traversal: STUN (discover a
  peer's public address) and, when direct connection fails outright
  (symmetric NAT, restrictive firewalls), a TURN relay fallback — which
  *does* route data through a server. LagDaemon.com is the natural place
  to host both. This is a real requirement, not a detail to hand-wave:
  some meaningful fraction of real-world connections will need the TURN
  fallback, so "peer to peer" can't be the only path that works.
- WebRTC itself (DataChannels specifically) is a reasonable concrete
  choice even for a native JUCE app — it already solves signaling-driven
  NAT traversal and gives encrypted-by-default channels (DTLS) for free,
  via an embeddable library (e.g. libdatachannel) rather than needing to
  reinvent connection negotiation from scratch.

## 2. The hard part, stated honestly: what actually gets synchronized

Real-time collaborative editing of DCC content (3D scenes, audio
timelines, video edits) is a fundamentally harder problem than
real-time text collaboration (Google Docs-style). Text has cheap,
well-understood CRDT/OT algorithms; a 3D scene graph, an audio timeline,
and a video edit each have different natural units of conflict, and
none of them have an off-the-shelf "just use this algorithm" answer the
way text does. This spec does not pretend to solve that in one pass —
it splits the problem into what's tractable now and what's a real
research/design question later.

**Recommended MVP scope — presence + ownership, not general merging:**
- **Presence/awareness**: who's in the session, live cursor position,
  current selection, what object/asset someone currently has open —
  cheap, ephemeral, broadcast-style state that doesn't need conflict
  resolution because it's not persisted, just displayed.
- **Per-object/per-asset edit ownership** (a lock or "checked out by"
  model): rather than solving simultaneous free-form editing of the same
  object, a participant explicitly takes ownership of an object/asset
  before editing it; others see it's locked and by whom. This sidesteps
  the hardest merge-conflict problem entirely for v1, at the cost of not
  being truly simultaneous multi-editor-per-object. Real professional
  precedent for this tradeoff exists (most DCC/collab tools that aren't
  text editors lean toward ownership/locking over free-form merge for
  exactly this reason).
- **Stretch/later**: richer real-time merging for specific, narrower
  cases where it's actually tractable (e.g. two people moving different,
  non-overlapping objects in the same scene need no lock at all — only
  edits to the *same* object need ownership) — but this is a refinement
  on top of the ownership model, not a replacement for it, and should
  not block shipping the MVP.

## 3. Architecture placement

Given the Suite's established pattern (shared vocabulary, app-local
relevance — see the AssetSystem's Tier 1/2/3 split referenced elsewhere
in the Suite's design memory), the transport/session/presence layer
should be app-agnostic, living in something like a new `shared/Collab`
library (or extending `shared/Interop`) — while what actually gets
synchronized (a Transform in Engine, a timeline clip in Movie, a track
event in Station) stays app-specific, consuming the shared layer rather
than the shared layer knowing about any one app's data model.

## 4. Explicitly open, not decided here

- Session/room lifecycle: does a collaboration session map 1:1 to a
  Suite project, or can multiple projects share a session?
- Authentication/identity model for LagDaemon.com collaboration
  accounts — same question raised for Hermes publish auth; worth
  deciding once, shared by both systems rather than twice.
- Whether project history/versioning (who changed what, undo across
  participants) is in scope for v1 or a later pass.
- Bandwidth/cost model for the TURN relay fallback path, since that one
  *does* cost LagDaemon.com server resources per session that needs it.
