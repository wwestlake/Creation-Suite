# Virtual Engineer

The one way any app in the suite asks the AI. The AI belongs to the suite: its
accounts and keys are the suite's (`ai-settings.json` in the VFS, entered once
in the suite's settings) and its guidance is LiteSemRAG cards
(`shared/LiteSemRag`). An app supplies only its panel, its own context and its
own API endpoints.

## VirtualEngineer

`VirtualEngineer engineer(SuiteAppDomain::texture)`; set `appContext` (a short
description of what is open) and `setProjectId` when a project opens.
`ask(prompt, completion)`:

1. reads the suite's AI account for this app (no account: it says to add one in
   the suite's settings);
2. brings up the cards that match (shipped, suite, this app, the open project),
   by meaning when the account's provider has a measured embedding model, else
   on words (`shared/LiteSemRag/README.md`, Retrieval);
3. sends the cards (system prompt), the app's context, the last few exchanges
   and the request;
4. delivers the reply on the message thread, with details: model, account,
   which cards were used and how they matched (`matchedBy`, each card's
   `meaning` and `words`, `wordsOnlyBecause`), timing.

One request at a time; `cancel()` stops the running one between calls (its reply
says what was done before it stopped). `retrieveFor`
(blocking) and `retrieveAsync` (delivered on the message thread) give the cards a
request would bring up without asking the AI anything; matching by meaning still
embeds the request.

## Acting: tools

An app lets its engineer act by registering tools (`Tools.h`,
`docs/architecture/Suite-Agent-Runtime-Spec.md` section 5) and the state they
change:

```cpp
engineer.addStateDomain({ "graph", captureGraph, restoreGraph });
engineer.addTool({ "texture.graph.add_node", "Add a node", "Adds a node of a type...",
                   parametersSchema, creation::agent::Effect::write },
                 [this](const juce::var& args, auto done) { /* the editor's own calls */ done(result); });
```

With tools and a provider that can be sent them (OpenAI-style today), `ask`
runs the loop: the model calls tools, each call is checked against its schema,
gated by its effect and run on the message thread, and its result goes back,
until the model answers or a limit (`limits`: model calls, tool calls, the same
call repeating) or Stop ends the run.

| Effect | Runs |
|---|---|
| `read` | at once |
| `write` | at once; undone with the request |
| `destructive` | after the user allows it, in the app (`approver`; `EngineerChat` asks) |
| `external` | after the user allows it; not undone with the request |

Before a request's first change the state domains are captured; "Undo last
request" (`undoLastRequest`) restores them, and asks first if the work changed
after the request. A write tool is refused while no state domain is registered.
A request's record, with every action, goes into the open project
(`Agent/runs.json`, the latest 50). A provider that cannot be sent tools gets a
plain request and the reply says so (`toolsUnavailable`).

`EngineerChat` shows each reply with its actions and cards, Stop while a request
runs, and Undo last request.

## AgentApi

A local HTTP API on an app's engineer, for developer tools and other agents:
loopback only, a random port, a bearer token per run. It announces itself as
the VFS suite entry `agents/<app>.json` (`baseUrl`, `token`, `app`), never a
file on the OS, and removes it when it stops.

| | |
|---|---|
| `GET /v1/status` | the app, its open project, busy, the app's endpoints |
| `POST /v1/messages` | `{"content": "..."}` → `202 {"requestId"}` |
| `GET /v1/requests/{id}` | `queued` / `running` / `completed` / `failed`, with `response` or `error`, and `details` |
| `POST /v1/cancel` | stops the running request |
| `GET /v1/tools` | the tools the engineer may use, with their effects and parameters |
| `POST /v1/undo` | undoes the last request that changed something (`{"evenIfEditedSince": true}` also after later edits) |
| `GET /v1/cards/match?q=...` | the cards a request would bring up and how each matched; the AI is not asked |
| `GET /v1/app` | the app's endpoints |
| `GET /v1/app/{name}` | one of them (`addAppEndpoint`, before `start`) |

Ported from the FrustIDE research agent's `LocalAgentApi` (FrustLang
`projects/02_juce_language_host`).

## Tests

`tests/VirtualEngineerSmoke.cpp` runs against a stand-in provider, in its own VFS
scope: cards and context going out, matching by meaning with hand-made
embeddings (a card embedded once, words alone when embedding fails), the
conversation, one request at a time, no account, and every API endpoint. It
checks the real AI settings were not touched.

`tests/AgentRunSmoke.cpp`: the engineer acting, the scenarios of the spec's
section 14 against a scripted model and a small in-memory host - direct,
correct, policy and injection, undo, undo after an edit, limits, Stop, a tool
that answers later, a provider without tools, and the API's tools and undo.

`tests/MeaningLiveCheck.cpp` (`creation_suite_meaning_live_check`) is not a test:
it asks the real suite account for embeddings of sample cards and requests and
prints the similarities, which is what a model's floor and margin are set from.
