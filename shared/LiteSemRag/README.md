# LiteSemRAG cards

Guidance for the suite's Virtual Engineer, as small retrievable cards: a rule the
user gave, how a piece of work is done, what a tool is for, a fact about FRust.
Each request brings up only the cards that match it, so the guidance can grow
without filling the model's context. Ported from the FrustIDE research agent
(FrustLang `projects/frust-ide-agent`), same card format.

## Cards

`Card` (`Cards.h`): `id`, `kind` (`rule`, `process`, `tool`, `knowledge`,
`personality`), `title`, `text`, `tokens` (words that bring it up), `priority`
(0..100), `status` (`active` / `retired`), and for process cards `trigger`,
`authority`, `steps`, `gates`, `evidence`, `escalation`.

## Scopes

| Scope | Where | Who changes it |
|---|---|---|
| shipped | built into the suite from source | reviewed source edits only |
| suite | VFS suite entry `cards/suite.json` | the user (Cards panel), the agent on an explicit rule |
| app | VFS suite entry `cards/app/<app>.json` | same |
| project | project entry `Cards/cards.json`, inside the project | same |

Nothing is a file on the OS (`docs/architecture/Suite-Shared-Project-Model.md`).
A card id is one card across scopes: the closest scope's version wins (project,
app, suite, shipped), so a project can sharpen a suite rule, or retire it by
repeating its id with `status: retired`.

## Retrieval

`retrieve(query, scopes, maxCards, meaning)` (`Retrieval.h`). A card applies by
**meaning** or by **words**; personality cards always apply.

- **Meaning** (`Embeddings.h`): each card is embedded once from its title, kind,
  trigger, tokens and text, and the request every time; a card applies when its
  similarity is at least the model's floor and within its margin of the closest
  card. The embeddings come from the suite's AI account
  (`creation::services::SuiteAiEmbeddingClient`), only for a model the suite has
  measured: today OpenAI's `text-embedding-3-small` (floor 0.18, margin 0.08).
  Any other provider, a failed call, or no account: words alone, and the
  retrieval says why (`wordsOnlyBecause`).
- **Words**: the query's words (lower case, no filler, longer than two letters,
  at most 12) that start a word of the card's id, title, kind, text or tokens -
  "struct" finds "structs", not "construct".

Order: higher priority first; then the closer match, where one of the card's own
`tokens` outranks any meaning (an exact name - a keyword, a node, an error code -
must beat a near meaning), then meaning, plus a little per other word found; then
the closer scope. At most 6. The result's `context` is the text for the prompt,
with process cards' steps, gates, evidence and escalation.

Card embeddings are kept in the VFS suite entry `cards/embeddings/<model>.json`,
keyed by the model and the card's exact words: an edited card is embedded again,
an unchanged one never. It is a cache - losing it costs one embedding call.
`creation_suite_meaning_live_check` (in `shared/VirtualEngineer`) prints real
similarities from the suite account for sample requests and cards; a new model's
floor and margin are set from it.

## Tests

`tests/LiteSemRagSmoke.cpp`: retrieval with hand-predicted results, validation,
the stored form, and the store in the VFS. It runs in its own VFS scope
(`SuiteVfsJsonStore::setScopeForTesting`) and a project it deletes afterwards,
and checks the real suite cards were not touched.
