# Creation Suite Shared Extraction Plan

Date: July 28, 2026

## Why This Exists

The suite already has the right high-level architecture goal:

- one suite-owned VFS and shared resource model
- one FRust language core across apps
- one BYOK and AI provider system across apps
- one suite configuration surface
- one shared shell UI where the apps should look and behave the same

What the code actually shows today is only partially aligned:

- `Creation Station` still owns the main suite settings UI
- `Creation Movie` has its own placeholder suite settings window
- the suite header exists as duplicated local copies
- FRust and LLVM ownership still lives primarily in `Creation Engine`
- Station still carries app-local AI provider settings and BYOK wiring

This plan makes the suite repository the real source of truth.

## Shared Library Map

### `shared/AssetSystem`

Owns:

- suite settings storage paths
- virtual file system primitives
- asset identity and asset metadata contracts

Already real:

- `creation_suite_asset_system`

### `shared/UI`

Owns:

- shared app shell header
- shared transport shell presentation
- shared suite settings panel shell
- shared account chip and shell styling primitives

Now started:

- `creation_suite_ui`

### `shared/Services`

Owns:

- BYOK provider catalog
- AI account definitions and routing settings
- future shared suite service registry
- future shared auth/session contracts

Now started:

- `creation_suite_services`

### `FRust`

Will own:

- FRust frontend
- FRust runtime and JIT core
- shared LLVM-facing build contracts
- shared domain policy hooks

Build target now scaffolded:

- `creation_suite_cel`

### `shared/NodeSystem`

Will own:

- app-agnostic node graph substrate
- graph serialization contracts
- shared node catalog contracts used by FRust-backed apps

Build target now scaffolded:

- `creation_suite_node_system`

### `shared/Interop`

Will own:

- cross-app project registry (discovery, not ownership -- any app can open any project)
- shared asset reference rules (kind-based capability: an app uses what it understands in a project it has open)
- compatibility and versioning rules
- multi-process write coordination for one project open in more than one app at once -- see docs/architecture/Suite-Shared-Project-Model.md

Build target now scaffolded:

- `creation_suite_interop`

## Extraction Order

### Pass 1: Shell And Settings

Move first:

- shared suite header
- shared suite settings panel shell

Reason:

- these are already duplicated and user-visible
- this directly fixes the "same suite window should open everywhere" problem

### Pass 2: Suite Configuration Expansion

Expand the suite control surface into sections:

- General
- Storage and VFS
- AI Providers
- AI Accounts and API Keys
- Integrations and Control Surfaces
- Licensing and EULA
- App Locations and Project Homes

The current six-path form is too small for the suite's real responsibility.

### Pass 3: BYOK And AI Consolidation

Move from app-local ownership to suite ownership:

- provider catalog
- provider-specific defaults
- stored accounts
- default model selection
- shared throttling and request policy

Provider planning baseline:

- OpenAI
- Anthropic
- Google Gemini
- xAI
- OpenRouter
- Groq
- Mistral
- Cohere
- Fireworks
- DeepSeek
- Together
- Perplexity
- Azure OpenAI
- Ollama
- custom OpenAI-compatible endpoints

### Pass 4: FRust And LLVM Consolidation

The suite should own the shared FRust stack, with apps supplying domain policy and domain intrinsics.

Move or mirror from `Creation Engine` into suite ownership:

- FRust frontend
- FRust runtime core
- LLVM discovery helper
- domain gating contracts
- node-to-FRust shared contracts

Keep per app:

- app-specific intrinsic domains
- capability policy
- host wiring

### Pass 5: Interop Contracts

Add suite-owned contracts for:

- shared asset IDs
- immutable version references
- "track latest" references
- FRust library references
- non-destructive source preservation

### Pass 6: Declarative 3D Scene Asset System

Add a suite-native spatial asset family for editable 3D content:

- declarative scene/model source schema
- canonical suite scene IR
- shared validation and normalization
- Suite VFS asset references
- provenance for prompt-authored and user-authored scene sources
- derived runtime products for Engine, Movie, Live, and any future suite consumers

This should support authoring paths from:

- direct user editing
- AI-generated scene/object descriptions
- node graph generation
- FRust-backed procedural generation
- imports from interchange formats such as glTF

The important architectural rule is that interchange formats are not the only truth. The Suite keeps its own editable scene description as the preserved source of record.

## Immediate Code Tasks

1. Wire `Creation Station` to consume `creation_suite_ui` for the suite header and suite settings panel.
2. Wire `Creation Movie` to consume `creation_suite_ui` and delete the local suite settings placeholder.
3. Extract Station's app-local AI provider settings into `shared/Services`.
4. Add a suite-level LLVM helper so Station, Movie, Live, and Engine stop drifting.
5. Begin moving FRust core targets under `FRust`, with Engine remaining the reference implementation during the move.
6. Define the suite-native declarative 3D asset schema and shared compiler/normalizer layer under a new shared spatial asset module.

## Guardrails

- Suite-owned code must not be re-copied into app-local folders.
- App code may wrap shared components, but should not fork their implementation.
- Shared FRust remains domain-neutral; apps add capabilities through policy and intrinsics.
- Shared VFS and interop rules must never destroy original artist source material.
