# Creation Suite Editor — Functional Specification

Status: planning only, nothing implemented under this name yet. A real, working reference implementation already exists inside Creation Engine (`Source/Views/ScriptPanel.h`, `Source/Views/CelCodeTokeniser.h/.cpp`) and should be the extraction source, not a from-scratch build.

## 1. Overview And Core Philosophy

Every suite app that touches FRust, JSON, or Markdown currently has to solve text editing on its own — or worse, not solve it and get a plain `juce::TextEditor` with no syntax awareness. Creation Engine already built a real FRust-aware editor (tokeniser + script panel) for its own use. Per the suite's shared-first design rule (see [[Shared-First-Feature-Design]]), that's exactly the kind of capability that should not be re-invented per app.

This spec proposes a **shared editor core** — not a new standalone "Creation X" app — that any suite app can embed: Engine's FRust script panel, Station's Patina DSL editor, Texture's material-graph FRust nodes, and wiki/doc editing anywhere in the suite (including, pointedly, the very `.md` files this session has been hand-editing all day).

Guiding principle: **the editor has one core, and per-format intelligence plugs into it.** Adding a new format means writing a language-service plugin, not a new editor.

## 2. Format Support Matrix

| Format | Depth | Backing |
|---|---|---|
| FRust | Deep — real tokens, real diagnostics, real symbols | `FRust` frontend (lexer, `sema.h`, `diagnostics.h`) |
| JSON | Schema-aware | Validates against suite manifest schemas (`ProjectManifest`, `AssetCatalog`, etc.) where a document's type is known |
| Markdown | Structural + preview | Heading/link/code-fence aware; live preview pane; this is the format the suite's own wikis and docs are written in |
| Math notation (within Markdown) | Preview-rendered | KaTeX — inline `$...$` and block `$$...$$` LaTeX math rendered in the Markdown preview pane |
| Plain text | Baseline | Word-level editing only, no tokeniser |

Non-goal: this is not meant to become a general-purpose C++/GLSL IDE. Other languages can get a tokeniser later if a real app need shows up, but FRust/JSON/Markdown/plain-text is the actual scope.

## 3. Code-Level Editing Capabilities

- syntax highlighting via a per-format `CodeTokeniser` (FRust's `CelCodeTokeniser` in Engine is the reference to extract, not reinvent)
- real diagnostics, not guesses: for FRust, run the shared `ce::lang::DiagnosticEngine` (lexer/parser/sema) on edit-pause or save and surface `Diagnostic{code, severity, loc, message}` as inline squiggles + a problems list — this is a real compiler frontend already, not a heuristic linter
- bracket/paren matching, auto-indent, code folding
- symbol outline / go-to-definition for FRust, sourced from `sema` output, not regex
- multi-cursor editing
- find/replace with regex support

## 4. Word/Prose-Level Editing Capabilities

- soft word-wrap toggle, independent of code mode
- word/sentence/paragraph-granularity cursor movement and selection (not just character/line)
- spell-check hook (pluggable dictionary; not blocking on a specific implementation choice yet)
- Markdown live preview pane, toggleable split or full-swap
- KaTeX math rendering inside the Markdown preview: `$inline$` and `$$block$$` LaTeX notation renders as typeset math, not raw text — useful for the suite's own technical specs and any app docs/notes that need formulas (DSP math in Station's Patina docs, transform/kinematics math in Modeler/Engine notes, etc.)
- word-count / reading-time status for prose documents
- track-changes-lite: a simple word-diff view for comparing two revisions of the same VFS-backed document (useful for wiki/doc review, not a full version-control UI)

## 5. Architecture

Built as a shared JUCE component library, `shared/Editor`, on top of `juce::CodeEditorComponent`/`juce::CodeDocument` rather than replacing them — the goal is a suite-standard *shell* around JUCE's editing primitives, not a new text-editing engine.

```text
shared/Editor/
  include/creation/editor/
    EditorDocument.h       # VFS-backed document, not a raw file path
    LanguageService.h      # plugin interface: tokenise(), diagnose(), symbols(), complete()
    CelLanguageService.h   # wraps FRust frontend
    JsonLanguageService.h  # schema-aware JSON support
    MarkdownLanguageService.h
    MarkdownPreviewRenderer.h  # Markdown -> HTML, with KaTeX math pass
    SuiteEditorPanel.h     # the actual embeddable JUCE component
  src/
    ...
```

`LanguageService` is the plugin seam:

```cpp
class LanguageService {
public:
    virtual juce::CodeTokeniser* GetTokeniser() = 0;
    virtual std::vector<Diagnostic> Diagnose(const juce::String& text) = 0;
    virtual std::vector<Symbol> Symbols(const juce::String& text) = 0;
    virtual std::vector<CompletionItem> CompleteAt(const juce::String& text, int offset) = 0;
};
```

`SuiteEditorPanel` picks a `LanguageService` by file extension/content-type at open time and otherwise stays format-agnostic — same panel class whether it's editing a `.frust` script or a wiki `.md` page.

## 6. Suite Integration

- documents open/save through `shared/AssetSystem`/VFS like every other suite asset — a `.frust` script, a project's JSON manifest, and a wiki page are all suite assets with identity, versioning, and provenance, not bare file-path edits (consistent with [[System-Architecture]] §1–2)
- FRust diagnostics come from the real shared frontend once Engine's FRust migration lands (see [[Roadmap]] Phase 4) — until then, this can temporarily depend on Engine's local `Language/` build as a stopgap, clearly marked as such
- AI-assist hooks route through `shared/Services` (`SuiteAiService`) for "explain this error," "suggest a fix," or "draft this doc section" — reusing the suite's existing AI/BYOK plumbing rather than a bespoke integration

## 7. Relationship To Existing Code

Creation Engine's `Source/Views/ScriptPanel.h` and `Source/Views/CelCodeTokeniser.h/.cpp` are a **real, working reference implementation** of exactly this idea, scoped to one app. The recommended path is extraction, not parallel construction:

1. Move `CelCodeTokeniser` into `shared/Editor` as the FRust `LanguageService`'s tokeniser, generalizing anything Engine-specific.
2. Build the generic `SuiteEditorPanel`/`EditorDocument`/`LanguageService` shell around it.
3. Add `JsonLanguageService` and `MarkdownLanguageService`.
4. Cut Engine's `ScriptPanel` over to consume `shared/Editor` instead of owning its tokeniser locally — same pattern as every other shared-extraction in [[Roadmap]].
5. Offer the panel to Station (Patina), Texture (material-graph FRust), and wiki/doc editing wherever the suite ends up wanting an in-app markdown editor.

## 8. Non-Goals

- not a general-purpose IDE (no project-wide refactoring, no arbitrary-language plugin marketplace)
- not LSP-protocol-compatible in v1 — the `LanguageService` interface is in-process/native, not a language-server client; revisit only if a real cross-process need shows up
- not a replacement for VS Code or other external editors for suite developers writing C++
- not a full version-control UI — the word-diff view in §4 is a lightweight comparison aid, not a merge tool

## 9. Explicitly Not Decided Yet

- whether this ships its own standalone thin app (a "quick edit a suite asset" utility window) in addition to being embedded, or stays embed-only
- spell-check dictionary/library choice
- exact suite manifest schemas to validate JSON against beyond `ProjectManifest`/`AssetCatalog`
- whether Markdown preview renders through a suite-shared renderer or a minimal built-in one
- **KaTeX rendering mechanism** — JUCE has no native LaTeX/MathML renderer, so this needs one of: (a) an embedded `juce::WebBrowserComponent` showing HTML with KaTeX's JS/CSS bundled locally (no CDN dependency, works offline), rendering the whole Markdown preview as HTML+KaTeX together, or (b) KaTeX running headless (e.g. via a bundled JS engine) to rasterize just the math spans to images/SVG that get composited into an otherwise-native-rendered preview. (a) is far less work and is the default recommendation; (b) only worth it if the rest of the preview stays native-JUCE-rendered for some other reason. Either way, KaTeX assets ship bundled with the app — never fetched from a CDN at runtime.
- final shared module name/target (`shared/Editor` / `creation_suite_editor` used above is a proposal, not locked)
