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

`retrieve(query, scopes)` (`Retrieval.h`): the query's words (lower case, no
filler, longer than two letters, at most 12) are matched against each active
card's id, title, kind, text and tokens; personality cards always apply. Higher
priority first, then the closer scope; at most 6; the result's `context` is the
text for the prompt, with process cards' steps, gates, evidence and escalation.

## Tests

`tests/LiteSemRagSmoke.cpp`: retrieval with hand-predicted results, validation,
the stored form, and the store in the VFS. It runs in its own VFS scope
(`SuiteVfsJsonStore::setScopeForTesting`) and a project it deletes afterwards,
and checks the real suite cards were not touched.
