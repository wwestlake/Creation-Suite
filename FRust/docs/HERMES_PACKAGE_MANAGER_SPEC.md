# Hermes — Frust Package Manager & Distribution System

Rewrite of the original CEL-era spec (`shared/CEL/docs/HERMES_PACKAGE_MANAGER_SPEC.md`,
written 2026-08-02, deleted in `2206eeb` when `shared/CEL` was removed during
the CEL→Frust migration, and never recreated at this path despite the doc
index in `AGENTS.md` pointing here since). This version reflects Frust as it
actually exists today (2026-09-16), not CEL, and states what's been directly
verified in the real codebase rather than repeating the original's plan as if
it were still speculative.

Named by the user: Hermes, after the Greek god of trade, travelers, and
roads. The name is a concept, not a separate product — **`frate` (in
`FrustLang/projects/05_frate`) is the actual, current implementation of the
Hermes vision.** There is no other tool to build; where this doc says
"Hermes," it means `frate`.

## 1. Confirmed real, today

- **A live hosted registry at lagdaemon.com.** `GET /djehuti/api/frate/pods`
  lists every published pod; `GET /djehuti/api/frate/pods/{name}/{version}`
  resolves one. As of this writing it holds 20 real pods (`core`, `meta`,
  `plugin`, plus `frust_linalg`, `frust_noise`, `frust_geo`, `frust_numerics`,
  `frust_dsp`, `frust_wave`, `frust_osc`, `frust_ecs`, `frust_graph`,
  `frust_physics`, `frust_hep_stats`, `frust_histogram`, `frust_json`,
  `frust_collections`, `frust_net`, `frust_http`, `frust_websocket`).
- **`frate`'s registry client is real, not a stub.** `FrateRegistryClient.cpp`
  (`FrustLang/projects/05_frate/src/`) implements `searchPods()` (hits
  `/pods` directly), `getDownloadUrl()` (resolves a presigned S3 redirect from
  `/pods/{name}/{version}`), `downloadFromS3()`, and the publish-side
  `getUploadUrl()`/`uploadToS3()`/`publishPod()`. `main.cpp` instantiates it
  against the live lagdaemon.com registry by default.
- **A real local cache exists** (`FrateCache.cpp`) — a downloaded/built pod is
  stored under a shared cache location and reused, not re-fetched per
  project. Confirmed on disk during this session (e.g.
  `bin/Debug/cache/frust_noise/0.1.0/frate.json`).
- **Dependency resolution exists** (`FrateResolver.cpp`), wiring a project's
  declared dependencies to the registry client and the cache.
- **A manifest format exists and is in real use**: `frate.json` per pod
  (`name`, `version`, `type`, `description`, `exports`, `dependencies` with
  name+version), confirmed across all 20 published pods. This answers the
  original spec's §5 open question — no separate Hermes manifest was
  invented; `frate.json` is it.
- **Publishing requires a Bearer token** tied to a signed-in IDE session
  (`frate publish` failing with "Not signed in" directs the user to the IDE
  Account menu) — partially answers the original §5 auth question, though
  namespacing/moderation policy is still undecided.

## 2. Distribution format — settled, not still open

The original spec flagged a real tension (source vs. intermediate-AST vs.
true pre-compiled binary) and recommended against a binary format without
deciding outright. That recommendation is now confirmed correct by Frust's
own operating model, restated directly by the user: **Frust's whole point is
live coding, compiling, and running inside the host app that's already
running** — not a separate ahead-of-time build artifact loaded like a
conventional plugin binary. A pod is source (or, as an optimization,
serialized AST with parsing skipped) that gets merged into the *same*
whole-program JIT compile as the consuming project's own generated code, at
build time, inside the running app. There is no second, binary-loading
execution model living alongside the JIT one, and none should be built.

## 3. The actual build flow (what "hit BUILD" means)

For an app like Djehuti Texture, the intended flow is:

1. User authors a schematic (node graph) in the app's own editor.
2. User hits Build. The app generates Frust source from the graph — the same
   proven pattern already used elsewhere in the suite (Foley/Signal Lab's
   `CompileBehaviorGraphToFrust`, genuinely verified working end-to-end via
   `AudioGraphSelfTest.cpp`).
3. The app's build step needs to run the equivalent of `frate build` against
   that generated source: resolve the schematic's pod dependencies (declared
   via `use` imports and/or a generated `frate.json`), fetch anything not
   already cached from the registry via `FrateRegistryClient`, merge
   everything into one compile unit.
4. The result is JIT-compiled and loaded live via `frust_plugin_host` /
   `creation::frust::PluginRuntime`, already used elsewhere in the suite
   (Foley, CreationEngine's `PodEditorPanel`/`EngineNodeLibraryLoader`).

**What is NOT yet confirmed:** whether any Suite app actually invokes this
pipeline from its own in-app "Build" action today. `frate` the tool is real
and working at the CLI/registry-client level (§1); the app-side integration
that calls it from a live editor's Build button has not been checked. This is
the concrete remaining gap for Texture's pod-plugin system, not the registry
or the fetch/cache mechanics.

## 4. Live-reload safety — a real, separate, unresolved engineering item

Directly relevant once pods are loaded live inside a running app (not just at
a one-time startup load): a same-session audit of `frust_plugin_host` and
`creation::frust::PluginRuntime` (2026-09-16) found:

- The underlying C API (`FrustPluginHost.cpp`) is internally thread-safe — a
  single process-wide `std::mutex` guards load/unload/reload/get_fn/events/
  services, confirmed via source read, with a real 8-thread stress test
  (`concurrency_stress_example.cpp`) exercising the events/services
  registries.
- `frust_plugin_call_on_event` takes that same mutex on every single call
  (it resolves the function pointer via `frust_plugin_get_fn` internally each
  time) — acceptable for a UI-triggered rebuild, but a real concern if a pod
  is ever called from a genuine audio-callback thread, where blocking on a
  mutex risks priority inversion.
- `creation::frust::PluginRuntime`, the C++ wrapper apps actually use, has
  **no synchronization of its own** — its `plugins`/`errors` maps are plain
  `std::map` with zero locking. Concurrent `load()`/`reload()`/`unload()` vs.
  `callEvent()`/`getFunction()` from different threads races at this layer
  independent of the C API below it.
- A previously-resolved function pointer becomes dangling immediately on
  `reload()`/`unload()`, with no refcounting or grace period — a caller
  mid-call on a stale pointer when a reload fires elsewhere is a genuine
  use-after-free, not just a glitch.

None of this blocks a UI-thread-only "hit Build, recompile, swap in the
result" flow (Texture's case, most likely — image/procedural work, not a
hard-real-time audio callback). It does mean: do not assume this is safe to
call from a real-time audio thread without further work, and do not
casually reuse this exact pattern for Signal Lab's live-oscillator case
without addressing it first.

## 5. Open questions, carried forward from the original spec, still open

- Publisher namespacing/moderation policy for lagdaemon.com (flat global pod
  names today — confirmed by the current registry listing; no `owner/name`
  scoping exists).
- Offline behavior — what a project build does when the registry is
  unreachable and a dependency isn't already in the local cache.
- Whether a genuine pre-compiled binary distribution format is ever needed
  (see §2) — still recommend not building this unless a concrete case shows
  the JIT/optimization time itself, not parsing, is the real bottleneck.
