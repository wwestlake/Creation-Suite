# Suite Asset Type Catalog

## Purpose

This catalog defines the Suite-wide vocabulary for assets. It prevents each
application from inventing a private storage model or treating a loose OS file
as if it were already a Suite asset.

An asset is a versioned Suite VFS record with a stable identity. A project owns
its project assets. An application may ship read-only application assets, such
as templates, canonical rigs, icons, and starter content. Both use the same
descriptor, version, reference, dependency, licensing, and provenance rules.

The catalog uses a small set of durable top-level kinds. Applications extend a
kind with a namespaced JSON document schema; they do not add a private asset
store or a new top-level kind for every feature.

## Asset Envelope

Every asset version has a JSON descriptor and may have one or more payloads.
The descriptor is Suite-owned metadata. A payload can be JSON, text, or binary.

```json
{
  "assetId": "char-humanoid-robot",
  "versionId": "v3",
  "kind": "character",
  "scope": "project",
  "displayName": "Humanoid Robot",
  "payloads": [
    { "role": "source", "mediaType": "model/gltf+json", "path": "..." },
    { "role": "runtime", "mediaType": "model/gltf-binary", "path": "..." },
    { "role": "preview", "mediaType": "image/png", "path": "..." }
  ],
  "metadata": {
    "suite": {},
    "djehuti.engine": { "documentKind": "BaseCharacter" }
  },
  "dependencies": [],
  "provenance": {},
  "license": {}
}
```

`metadata.suite` is reserved for common fields. An application owns only its
own namespace, for example `djehuti.engine`, `djehuti.texture`,
`djehuti.movie`, or `djehuti.station`. A consumer that does not understand a
namespace preserves it unchanged.

## Source, Normalized, And Runtime Payloads

Import creates a root source asset; it never discards the external original
during import. The importer records the source tool, external source path when
applicable, import settings, license, and a content hash. It can then create
computed derivatives with explicit dependency edges back to that source.

Asset versions are immutable. Reimport, an external-tool update, a conversion,
or a new build creates a new version and new payload paths; it never overwrites
an existing VFS payload. References may intentionally track `latest`, or pin
an exact version when reproducibility matters.

Preservation is not retention policy. A user may delete an imported source,
its editable source-art payload, an application-package copy, or any derived
asset. Before completing the deletion, the Suite shows the dependency impact:
computed dependents are removed or marked invalid as appropriate, while
referential documents survive with an actionable missing-reference state. An
external original remains the user's own file; deleting its Suite asset never
deletes that external file unless the user explicitly chooses that separate OS
operation.

- `source`: original user or third-party file, preserved for reimport and
  editing in its native tool.
- `normalized`: Suite-approved interchange or editing representation.
- `runtime`: optimized application-ready payload.
- `preview`: thumbnail, waveform, turntable, or other derived inspection aid.
- `document`: JSON document that describes an authored assembly or policy.

Initial normalization policy:

| Family | Accept at import | Normalized payload | Notes |
| --- | --- | --- | --- |
| Image | PNG, JPEG, TGA, BMP, HDR and other supported formats | PNG for ordinary color and mask images; preserve HDR source until an HDR format policy is selected | Never silently reduce HDR data to PNG. |
| 3D model | glTF/GLB, FBX, OBJ and supported sources | GLB/glTF with Suite validation metadata | Preserve the source and all generated meshes, materials, skins, and clips as related assets. |
| Audio | WAV, AIFF, FLAC and supported compressed sources | WAV for editable/project audio; optional delivery codec later | Preserve sample rate, channels, and original source. |
| Video | supported video containers/codecs | Suite video editing/delivery profile, to be selected before implementation | Preserve the original and record codec, color space, frame rate, and audio streams. |

The normalized format is a policy choice, not a substitute for source
preservation. A game can consume a runtime derivative while Blender, GIMP, or
Reaper continues to edit the source through an integration bridge.

## Canonical Top-Level Kinds

### Media

| Kind | Meaning | Typical payloads |
| --- | --- | --- |
| `image` | Still image, texture, mask, HDR environment, or rendered frame | PNG, HDR, source image |
| `imageSequence` | Ordered frame set | JSON manifest plus image assets |
| `audio` | Audio recording, sample, stem, or rendered sound | WAV plus original source |
| `video` | Video source, render, or edited delivery | video container plus JSON stream metadata |
| `font` | Typeface or font family used by a project | font binary plus license metadata |

### 3D And World Content

| Kind | Meaning | Typical payloads |
| --- | --- | --- |
| `model` | Reusable geometric model; may contain a mesh hierarchy, materials, skins, and clips | GLB/glTF, source Blend/FBX, previews |
| `material` | Reusable surface definition and its texture references | JSON graph plus image references |
| `skeleton` | Canonical rig, bone names, sockets, bind pose, and validation data | JSON document and optional model payload |
| `animation` | Named clips, blend spaces, retarget data, or procedural motion definitions | JSON plus encoded curves/model data |
| `terrain` | Terrain definition, tile/chunk data, voxel fields, masks, and derived meshes | JSON manifest plus binary tiles/models/images |
| `particleSystem` | Reusable particle effect definition | JSON graph and referenced media/materials |
| `objectDefinition` | Reusable assembly of models, materials, components, and Pods | JSON document with asset references |
| `scene` | Composition of instances, hierarchy, authored world state, and editor-only helpers | JSON document with asset references |

### Characters And Gameplay

| Kind | Meaning | Detailed document kinds |
| --- | --- | --- |
| `character` | Reusable character content and character state contracts | `BaseCharacter`, `Garment`, `Recipe`, `Definition`, `CreatorPolicy`, `Roster`, `Instance` |
| `game` | Game configuration, player slots, policies, startup settings, and game references | application-defined JSON |
| `pod` | FRust node/POD graph and build metadata | behavior, processing, locomotion, or other POD schema |
| `script` | Source or compiled programmable logic that is not a POD graph | FRust source, generated code, build metadata |
| `instance` | Placement-specific state that references reusable content; never duplicates its source bytes | scene, timeline, or application-defined state |

### Music, Movie, And Production

| Kind | Meaning |
| --- | --- |
| `timeline` | Editable audio, video, or cinematic timeline with tracks and placements |
| `patch` | Synthesizer, processing, or signal-routing graph |
| `foleyPatch` | Foley-generation or performance setup |
| `trackerArrangement` | Tracker song arrangement and pattern data |
| `midi` | MIDI performance, sequence, or control data |
| `preset` | Named parameter set for a known system or tool |
| `samplePack` | Curated collection manifest that references individual audio assets |

### Data And Delivery

| Kind | Meaning |
| --- | --- |
| `document` | General structured authored document not owned by another domain |
| `metadata` | Import manifests, reports, analysis, validation, or auxiliary structured data |
| `dataset` | Training, reference, measurement, or other structured dataset |
| `binary` | Opaque payload with a declared media type when no better kind applies |
| `package` | Versioned distributable collection of assets, such as starter content or a DLC/content pack |

`binary` is an escape hatch, not a default. An importer should choose the
most specific stable kind available.

## Scope And Visibility

- `project`: owned by one project and visible to every application opening that
  project.
- `application`: installed read-only content shipped or maintained by an
  application. It appears in the same browser through a package mount, clearly
  labeled as application content.
- `suite`: a future shared library visible across projects, governed by the
  same VFS and version rules.

An application asset becomes a project asset only through an explicit
**Add to Project**, **Copy**, or **Derive** operation. That operation creates a
project-owned version with provenance pointing to its application asset source.
No app should silently place a hidden application file in a project scene.

## Current Token Migration

The current `AssetKind` enum is the implementation vocabulary, not yet the
final catalog. Existing tokens remain readable during migration:

| Current token | Catalog destination |
| --- | --- |
| `render` | Split by payload and purpose into `image`, `audio`, `video`, or `model`; do not use for new assets. |
| `audio` | `audio` |
| `patch`, `foleyPatch`, `trackerArrangement`, `midi`, `preset`, `samplePack` | Retain as production kinds. |
| `pod`, `objectDefinition`, `character`, `game`, `scene`, `instance`, `script`, `metadata`, `binary` | Retain with the definitions in this catalog. |
| `unknown` | Import failure or unclassified legacy data; not a publishable new asset kind. |

Adding `image`, `model`, `video`, `material`, `skeleton`, `animation`,
`terrain`, `particleSystem`, `timeline`, `font`, `document`, `dataset`, and
`package` requires one coordinated shared-AssetSystem schema update. Individual
apps then add their namespaced JSON schemas and import/export handlers.

## Browser Rules

The Suite Content Browser must show mounted project and application assets in
one queryable view, with visible scope and origin. It must never require a
designer to know an OS path merely to use installed Suite content.

For example, `Humanoid Robot` may ship with Djehuti Engine as an application
asset. It appears under Characters/Models with an `Application` badge. Choosing
**Add to Project** creates a project asset and makes it eligible for placement,
Character Recipe creation, and player-slot assignment.
