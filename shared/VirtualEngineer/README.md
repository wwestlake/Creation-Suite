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
2. brings up the cards that match (shipped, suite, this app, the open project);
3. sends the cards (system prompt), the app's context, the last few exchanges
   and the request;
4. delivers the reply on the message thread, with details: model, account,
   which cards were used, timing.

One request at a time; `cancel()` drops the running one's reply.

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
| `POST /v1/cancel` | stops waiting for the running request |
| `GET /v1/cards/match?q=...` | the cards a request would bring up, nothing sent |
| `GET /v1/app` | the app's endpoints |
| `GET /v1/app/{name}` | one of them (`addAppEndpoint`, before `start`) |

Ported from the FrustIDE research agent's `LocalAgentApi` (FrustLang
`projects/02_juce_language_host`).

## Tests

`tests/VirtualEngineerSmoke.cpp` runs against a stand-in provider, in its own VFS
scope: cards and context going out, the conversation, one request at a time,
no account, and every API endpoint. It checks the real AI settings were not
touched.
