# Suite Agent Loop — Virtual Engineer Orchestration

Quick-capture spec, written 2026-08-02, in the same design session as
the FRust v2 language work (`FRust/docs/FRust_V2_LANGUAGE_SPEC.md`) and
the Station control registry seed
(`apps/CreationStation/docs/STATION_CONTROL_REGISTRY_SEED.md`), which
this depends on as its actual mechanism for taking action. This document
answers "how does the Virtual Engineer actually decide and carry out
what to do," which none of the other specs written tonight cover — they
describe what it can eventually *do* (control state, run FRust), not how
it *decides* to do it, step by step, safely.

Tracking: not yet filed as GitHub issues — file once reviewed.

## 0. Why this exists — the alternative already rejected

A single LLM call that reads a user's request and emits one FRust script
in one shot is the wrong shape for anything beyond trivial commands.
It can't check its own assumptions before acting, can't verify the
result actually happened, and can't recover if a step fails partway
through a multi-step request. The alternative, confirmed directly: model
this the way an agentic coding tool actually works — not a proprietary
secret, a well-established, publicly-documented pattern — perceive,
reason, act one step at a time, observe the real result, verify or
iterate. This is orchestration logic the Suite owns; the LLM's job at
each step is a normal API call ("given this context and what just
happened, what's the next action?"), which works identically regardless
of which BYOK provider is configured (OpenAI, Anthropic, or anyone
else) — nothing about the loop itself is tied to one vendor.

## 1. The loop

```
1. PERCEIVE  — gather context before acting
2. REASON    — decide the next single step (not the whole plan at once)
3. ACT       — take exactly one action
4. OBSERVE   — look at the real result of that action
5. VERIFY    — does the observed result match intent?
     yes -> report back or move to the next step
     no  -> reason again, with the failure as new context
```

Repeats until the request is satisfied, genuinely blocked (needs the
user), or a step limit / safety bound is hit — FRust's own real-time-safe
profile is not the relevant analogy here (this loop is not itself
running in a real-time callback), but the same instinct applies: an
unbounded agent loop needs its own watchdog, not just FRust scripts.

### 1.1 Perceive — where context comes from

- **LiteSemRAG grounding**: the user's request, filtered through
  LiteSemRAG's index of the current project's state, resolves references
  like "the bass guitar track" to a concrete target (e.g. "track 3")
  *before* the reasoning step ever runs. This spec takes LiteSemRAG's
  existence and behavior as a given external input, per the user's own
  framing — not something to redesign here.
- **Current app state**: read via `get_state` (`FRust_V2_LANGUAGE_SPEC.md`
  section 6) — the loop should check current values before deciding an
  action, not assume.
- **Suite context**: whatever `SuiteContextEngine` already assembles
  today for a BYOK chat turn — this loop is a new consumer of that
  existing plumbing, not a replacement for it.

### 1.2 Reason — one step, not the whole plan

The model is asked for the *next single action*, not a full multi-step
plan up front. A plan can be too confident about steps 3-5 before steps
1-2's real results are known — deciding one step at a time, informed by
what actually happened, is the entire point of this loop over the
single-shot alternative. For a genuinely multi-step request ("set up a
new instrument track" = create + plugin + arm + monitor), the loop
tracks sub-steps explicitly (mirroring `TaskCreate`/`TaskUpdate`-style
tracking) so a long request can't silently drop a piece — this is
suite-level infrastructure, not something each app reimplements.

### 1.3 Act — exactly one FRust action per step

Each step compiles and runs one small, targeted FRust script (or a single
`set_state`/intrinsic call) against the live app instance — not a large
multi-page script attempting the whole request at once. Smaller actions
are individually verifiable; a large one-shot script either fully
succeeds or fails opaquely.

### 1.4 Observe — check the real result, not the LLM's claim

After acting, the loop reads back the actual state via `get_state` (or
whatever the action's natural read-side is) rather than trusting that
the action succeeded because the FRust script didn't error. A `set_state`
call that silently no-oped (e.g. a typo'd property name that happened
to still type-check) should be caught here, not assumed away.

### 1.5 Verify — confirm or iterate

Compare the observed result against what the user actually asked for.
If it matches, either continue to the next sub-step or report back to
the user (voice/UI confirmation — "bass guitar volume lowered"). If it
doesn't match, the mismatch becomes new context for another REASON step
— the loop adapts rather than blindly retrying the identical action.

## 2. Where this lives

Suite-level infrastructure, alongside `SuiteContextEngine`/
`SuiteAiProviderRuntime` (`shared/Services`) and `SuiteAiChatPanel`
(`shared/UI`) — this loop is the orchestration layer that sits between
"a BYOK LLM is reachable" (already built) and "the LLM can actually
carry out a multi-step, self-checking task" (not built yet). Consuming
apps (Station, Engine, Movie, Live) plug in their own `set_state`
registry entries and domain intrinsics; the loop itself should not need
to know which app it's running against.

## 3. Explicitly open, not decided here

- **Step/iteration bound**: what stops a stuck loop from spinning
  forever reasoning-acting-failing? Needs its own limit, analogous to
  (but distinct from) FRust's FRust9001 watchdog — this loop runs at the
  orchestration layer, not inside a compiled script.
- **Clarifying questions**: when is the loop genuinely blocked and
  should ask the user (voice or UI) rather than guessing? Mirrors the
  "ask only when genuinely blocked, not by default" instinct already
  used elsewhere, but the concrete trigger condition for this loop
  isn't defined yet.
- **Multi-turn session state**: does the loop's step-by-step history
  persist across a whole voice session, or reset per request? Affects
  whether "now also boost the mids" (a follow-up, no re-stated subject)
  can resolve against the previous step's target.
- **Failure reporting back through voice**: what does the loop say when
  it genuinely can't complete a request (a property doesn't exist in
  the registry, a real API error) — needs to be legible to a spoken
  response, not just a text error.
