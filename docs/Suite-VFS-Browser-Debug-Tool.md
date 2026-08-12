# Suite VFS Browser (debug tool)

A developer-only tab in the suite-level Settings panel for inspecting the VFS: what's
actually on disk under the configured VFS root, and what entries the VFS service reports.
Exists to help understand the suite's storage layout while working on it — not a
user-facing feature.

## Where it is

Open any suite app, click the gear icon in the shared header bar ("Open suite settings"),
and select the **VFS Browser** tab. Only present in **Debug** builds — the whole feature
(button, tree, viewer, backing code) is wrapped in `#if JUCE_DEBUG` in
[`shared/UI/include/creation/ui/SuiteSettingsPanel.h`](../shared/UI/include/creation/ui/SuiteSettingsPanel.h)
/ [`.cpp`](../shared/UI/src/SuiteSettingsPanel.cpp), so it compiles out of Release entirely,
not just hides at runtime.

## What it shows

Two independent branches, read live from whatever actually exists — the tool has no
hardcoded assumption about layout, so it stays correct across future reorganizations of the
VFS rather than needing to be rebuilt alongside one:

- **Filesystem tree**, rooted at `creation::suite::getSuiteRootDirectory(settings)` — the
  real directory tree on disk under the configured VFS root, lazily expanded as you open
  folders (plain `juce::File::findChildFiles`, not a cached snapshot).
- **Suite Entries (via VFS service)** — the flat list of logical entry paths the running
  `CreationSuiteVfsService` reports (`GET /suite/entries`, via the new
  `SuiteVfsServiceClient::listEntries()`), split into a tree client-side. These are the
  small JSON settings entries apps store through the VFS service (`station-settings.json`,
  `ai-settings.json`, etc.) — they live inside the suite root project's own container, not
  as loose files, so they don't show up in the filesystem tree above at all.

Selecting a file or entry shows its content in the right-hand pane, formatted by what kind
of file it looks like: pretty-printed for `.json`, indented for `.xml`, raw text otherwise.
Parse failures show the raw content with a note rather than failing silently.

## Known limitation

Real per-app/per-project containers (`.csproj` files — actual project storage) show up in
the filesystem tree as opaque binary blobs; the tool reports their size but doesn't browse
their internal contents. `creation::vfs::SuiteVolume::listFiles()` exists and could back
that view later, but opening a container that may already be mounted read-write elsewhere
(by the very app you're inspecting from) risks a real lock conflict — that needs its own
concurrency-safe design, not something bolted onto this tool as-is. Flagged here rather
than silently faked.
