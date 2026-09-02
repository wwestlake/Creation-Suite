# Suite Asset Pipeline Model

## The model, stated plainly

An asset is not a single file. It is a node in a pipeline. A root asset is
raw source — a dropped FBX, a WAV, a texture image — and it is read-only:
you never edit a root in place, you re-import it. Everything downstream is
produced by some process consuming one or more upstream assets, and that
output is itself an asset — addressable, listable, deletable, exportable,
exactly like the root. A compiled material's GLSL, a GPU mesh built from an
FBX, a baked-out mesh from a procedural modifier, a filtered texture, a
processed audio clip — all pipeline stages, all first-class assets.

This applies uniformly across every `AssetKind`. Mesh, texture, audio,
material — same model, same operations. Nothing here is render-specific or
audio-specific.

This has been the intended shape of `shared/AssetSystem` since the shared
project model was decided (`Suite-Shared-Project-Model.md`), but it went
unbuilt because there were no real consumers yet forcing the question. The
Creation Engine import/material work (2026-09-01/02) is that first real
consumer. This doc exists to settle the model before it gets bent to fit
one app's shortcuts.

## Two kinds of "derived"

A downstream asset relates to its inputs in one of two ways, and the
difference controls what happens on change or delete:

- **Computed** — the asset exists only because the process ran; it has no
  authored content of its own beyond "the input(s), transformed." A baked
  GPU mesh, a compiled shader blob. If an upstream input changes, a
  computed asset should propagate — regenerate automatically, since it's
  just the input restated. If an upstream input is deleted, the computed
  asset has no reason to keep existing either.
- **Referential** — the asset is itself authored content that happens to
  point at another asset as an input. A Material is real, hand-built
  (someone authored its node graph); it references a Texture asset. If the
  Texture disappears, the Material does not stop existing — it enters a
  missing-reference state, visibly, but survives.

## Three operations on the graph

1. **Propagate** — an upstream change walks forward through computed edges
   and re-runs the affected processes. It does not walk through referential
   edges the same way; a referential dependent is *notified* (its input
   changed / its input is missing) rather than *regenerated*.
2. **Delete-with-dependency-check** — deleting an asset first checks who
   depends on it. Default: warn and refuse. An explicit override allows it
   anyway. What happens to dependents follows the computed/referential
   split above (computed dependents cascade-delete or become invalid;
   referential dependents survive in a missing-reference state).
3. **Promote (bake)** — any node in the graph, including a computed one,
   can have its current resolved output materialized into a standalone
   asset with its dependency edges cut. It stops being "computed from
   upstream" and becomes an independent root. This is how "import a mesh,
   apply a procedural modifier, decide it's too slow, bake a static mesh
   and discard the modifier chain" works: bake promotes the modifier's
   output to a new root, and the old chain above it — now depended on by
   nothing — becomes deletable through operation 2 like anything else.

Import and Export are not part of the graph itself; they are the two
boundary operations. Import creates a root node from an external file.
Export takes any node's current resolved output (root or derived, doesn't
matter which) and serializes it to an industry-standard external format —
FBX/glTF/OBJ for models, PNG/etc. for textures, WAV/etc. for audio.
Export does not touch the graph; it's a read-only leaf operation on
whichever node you point it at.

## Update / reimport

A root asset's external source file can change outside the suite entirely
— the user re-exports it from Blender, edits the WAV in another tool,
whatever. Reimport re-reads that same external source and produces a new
version of the root, which then propagates per the rules above.

This requires the root to remember where it came from. Today it doesn't —
see the schema gap below.

If the external file is missing at reimport time (moved, deleted, on a
drive that's not mounted), the user is offered a native file-browse dialog
to relocate it; the located path becomes the new remembered source and the
reimport proceeds from there. Reimport is never silently blocked by a
missing file — it degrades to "ask where it went," not to a dead end.

**Decided**: this is a per-reference choice the user makes when they place
or select the asset, not a single fixed default. When using an asset, the
user picks either "current" (the reference tracks whatever the latest
version is, and updates flow through automatically) or a specific,
version-locked pin (that reference never changes, regardless of later
reimports). `AssetReferenceMode` (`exact` / `compatibleLatest` / `latest`,
already implemented in `AssetResolver.cpp`) is exactly this — `exact` is
the version-lock, `latest` is "current." Nothing new needed at the schema
or resolution layer; the remaining work is UI-only, giving the user that
choice at the point they select/place an asset (Phase 2+, not Phase 0).

## This is a graph, not a tree

Fan-in isn't a one-level special case — it compounds. A Model can depend
on four Materials. Each of those Materials can independently depend on
several other assets — textures, and some of those could themselves be
computed (a baked or procedurally generated texture, not a root). The
result is a general dependency graph, arbitrarily deep and wide, not a
simple parent-chain: a 3D scene is a fabric of these edges, not a tree of
them. Any traversal (delete-check, propagate, missing-reference display)
has to walk that full graph — it cannot assume one parent per node, or
assume any node's inputs are only roots.

## Schema gap against today's `AssetDescriptor`

`shared/AssetSystem/include/creation/assets/AssetTypes.h` today has:

- `sourceAssetId` / `sourceVersionId` — a single parent link. This is a
  parent *field*, not a parent *set* — it can express fan-out (one root
  feeding many dependents) fine, since each dependent just holds its own
  single link back. It cannot express fan-in: a Referential Material
  referencing three separate Texture roots at once (baseColor, normal,
  roughness) has no way to record "depends on all of these." Delete-check
  and propagate both require walking the full reverse-edge graph, which
  isn't possible until this is a set.
- No computed-vs-referential flag anywhere.
- No field for an external OS source path. `sourceApp`/`sourceTool` record
  *what imported it*, not *where it came from on disk* — nothing today
  lets reimport find the file again.

What needs to be added, all in `shared/AssetSystem` (suite-wide, not
Engine-local — every app sees this):

- Replace the single parent link with an N-ary dependency list (asset id +
  version id per dependency).
- A derivation-kind flag: `root` / `computed` / `referential`.
- `externalSourcePath` on root descriptors, updated whenever a reimport
  relocates the file.

`versionId` / `revision` / `derivedFromVersionId` already exist and are
already the right shape for "reimport creates a new version, old ones stay
recoverable" — no change needed there.

## What's already real (as of 2026-09-02, Creation Engine)

Confirmed by reading the current import pipeline, not assumed:

- `ImporterRegistry` + per-format `AssetImporter`s (glTF/GLB, FBX, OBJ,
  Audio, Texture) — extension-dispatched, OS drag-and-drop already works
  via `ImportPanel` (`juce::FileDragAndDropTarget`).
- Every importer already persists through `ProjectContentAssetStore` /
  `ProjectAssetService` into the real shared-suite `ProjectSession` — this
  is live, working infrastructure, not a stub.
- `ProjectSession` already exposes everything the graph-check and browser
  operations need at the storage layer: `listEntryPaths()`,
  `containsEntry`, `readEntry`, `removeEntry`, `upsertAssetDescriptor`,
  `removeAssetDescriptorByVersionId`, `commit()`.

What's missing and net-new (Engine-side consumption of this model):

- No content-browser UI (folders, listing, search) anywhere yet.
- No delete UI, and the Engine-local runtime `AssetCatalog` (the
  GPU-resident cache, distinct from `ProjectSession`) has no `Remove()` to
  evict a runtime resource when the persisted copy goes away.
- No first-import metadata popup (this app has no modal-dialog convention
  yet at all).
- No reimport/update UI, and no bake/promote UI.
- No export UI.
- `.blend` import — explicitly deferred, not a today item.

## Phased plan

**Phase 0 — schema** (`shared/AssetSystem`): dependency list, derivation
flag, `externalSourcePath`. Everything else depends on this existing
first. **Done** (2026-09-02) — `AssetDescriptor::dependencies` (an
`AssetDependency` set, not the old single `sourceAssetId`/`sourceVersionId`
field), `derivationKind`, `externalSourcePath`, plus
`AssetCatalog::findDependents()` for the delete-check walk. Verified
building in `CreationEngineEditor`.

**Phase 1 — Engine runtime plumbing**: `AssetCatalog::Remove()` (+
`AudioCatalog` equivalent) so delete can actually free a live resource,
not just the persisted copy. **Done** (2026-09-02) — both catalogs got
`Remove()`; `scene::AssetCatalog` also got `RemoveMaterial()`, and
`ownedTextures_` had to become keyed by asset name (was an unkeyed vector)
so a removed asset's owned GPU texture can actually be found and freed.

**Phase 2 — Content Browser panel** (new, Engine-local UI): folder
tree/list from `listEntryPaths()` + descriptors. Delete with a confirm
dialog, backed by the dependency check from Phase 0. This is also the
list the materials-editor texture-node picker (from the UE4-pattern
discussion) searches over — that work is blocked on this phase existing.
**Done** (2026-09-02), scoped down from the original "folder tree" to a
flat, name-filterable list — real folder hierarchy in the browser was
never resolved as in-scope for today and stays future work. Scoped to
models/textures/audio only: Materials aren't
persisted into `ProjectSession` at all yet (`MaterialGraphPanel` only
ever writes into the runtime catalog), so there's no durable Material
descriptor for this panel to show — that joins once the still-pending
Materials-editor redesign makes Materials real persisted assets.

**Phase 3 — First-import metadata popup**: shown once per new asset, not
on reimport. **Done** (2026-09-02), narrowed from the original field
guess after checking what's actually real: `FbxLoader.cpp` already passes
`target_unit_meters = 1.0f` to ufbx, which auto-normalizes source units
correctly today — no scale field needed. A "generate Material assets"
choice doesn't apply since Materials aren't persisted (see Phase 2 note).
Target folder isn't offered either — there's no folder UI yet to place
it into. What shipped: Name (editable, prefills from filename and also
becomes the runtime cache key + descriptor.displayName, kept in sync),
Description, and Tags — real fields that actually do something, via a
`juce::AlertWindow` with text editors. Textures skip the popup entirely
(`AssetImporter::NeedsImportMetadata()` returns false for
`TextureAssetImporter`), per "images may not need anything." Every
import today is effectively a first import — there's no reimport path
yet distinguishing "this asset already exists" (that's Phase 4), so the
popup currently shows on every drop of a metadata-needing kind, not just
truly-new ones; Phase 4 needs to add that exception.

**Phase 4 — Reimport / update**: uses `externalSourcePath` from Phase 0.
Missing-file relocate flow via native file chooser. **Done** (2026-09-02)
— `AssetImporter::Reimport()` per format, `ProjectAssetService::
createNewVersion`, `ContentBrowserPanel`'s Reimport button with a
`juce::FileChooser` relocate fallback when the remembered source is
gone.

### Phase 5 — Bake / promote

**Status: honestly blocked, scoped down rather than built speculatively.**
Nothing in Creation Engine today produces a *computed* asset —
Materials aren't persisted (Phase 2's own note), and there is no
procedural-modifier system of any kind. Building an attach-to-object
Bake button now would be a control with nothing real to operate on.
Split into what's genuinely buildable today vs. what has to wait:

**5.1 — Generic Promote primitive** (`shared/AssetSystem`): a function
that takes an `AssetDescriptor`, clears `dependencies`, sets
`derivationKind = root`, and persists the change — the actual
"sever the edges" operation Section 5 of this doc describes. Verified
by a smoke test against synthetic descriptors (`AssetSystemSmoke.cpp`),
since there is no live computed asset to promote yet. No UI.
**Cycle**: start this sub-phase on a clean tree → build →
fix-and-rebuild until it passes → commit/push/PR → sync and confirm
clean before 5.2.

**5.2 — Consumer integration: explicitly not started.** Blocked on a
real computed-asset producer existing (a persisted Material, a
procedural mesh modifier, a baked Behavior pod — any of these would
do). Do not build UI for this speculatively; revisit once one of those
exists. **Cycle**: same as above, applies whenever this sub-phase
actually starts.

### Phase 6 — Export

Root assets already have their real, original file bytes preserved
verbatim in the project VFS (`ProjectSession::writeEntryFromFile`,
confirmed by `SHARED_EXTRACTION_PLAN.md`'s own guardrail: "Shared VFS
and interop rules must never destroy original artist source material").
That means exporting a root asset needs no format conversion and no new
writer library at all — it's a read of the stored bytes and a write to
wherever the user points, for every kind (model, texture, audio) the
same way. Confirm this at implementation time rather than assuming, the
same way every other phase's scope got checked against real code first.

**6.1 — Export a root asset's stored bytes as-is.** Resolve the asset
through `ProjectSession`, read its entry, write those exact bytes to a
user-chosen destination via a native save-file dialog. No `stb_image_
write` vendoring, no OBJ/glTF writer, no re-encoding — confirmed only
`stb_image.h` (reader) is vendored today, nothing for writing images,
and no model-writer code exists anywhere in this codebase; this
sub-phase's scope is deliberately chosen to need none of that. **Cycle**:
clean tree to start → build → fix-and-rebuild until it passes →
commit/push/PR → sync and confirm clean before 6.2.

**6.2 — Export UI**: an Export action per row in `ContentBrowserPanel`,
using 6.1's primitive. **Cycle**: same as 6.1, applied to this
sub-phase specifically before starting Phase 5.2 or closing out the
pipeline's original six-phase list.

**6.3 — Export of computed/derived assets (deferred, not started).**
Would need real format writers (`stb_image_write`, a glTF/OBJ writer)
since a computed asset's data doesn't already exist as a standard file
on disk the way a root's does. Blocked on the same producer gap as
Phase 5 — noted here so it isn't forgotten, not attempted now.

## Status

Phases 0–4 implemented and verified building in `CreationEngineEditor`
(2026-09-02). Phase 5 is intentionally scoped down to its buildable
primitive (5.1) with consumer integration (5.2) explicitly blocked on a
real computed-asset producer that doesn't exist yet. Phase 6's root-asset
export (6.1, 6.2) is ready to build; derived-asset export (6.3) is
blocked on the same gap as Phase 5.
