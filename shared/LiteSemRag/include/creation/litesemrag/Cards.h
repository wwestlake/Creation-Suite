#pragma once

#include <juce_core/juce_core.h>

// LiteSemRAG cards (shared/LiteSemRag/README.md): small, durable, retrievable pieces of guidance for the suite's
// Virtual Engineer - a rule the user gave, how a piece of work is done, what a tool is for, a fact about the language.
// Only the cards that match a request go into the model's prompt, so guidance can grow without filling the context.
// The format is the research one (FrustLang projects/frust-ide-agent/PROCESS_CARD_SCHEMA.md), unchanged.
namespace creation::litesemrag
{
// Where a card comes from, closest last. A card in a closer scope replaces a farther card with the same id - a
// project can sharpen, or retire, a suite rule just for itself.
enum class CardScope
{
    shipped, // built into the suite from source, read-only (the FRust language, the engineering process)
    suite,   // the user's, for every app and project
    app,     // the user's, for one app
    project  // the user's, for one project, stored inside it
};

juce::String scopeName(CardScope scope);            // "shipped", "suite", "app", "project"
bool scopeFromName(const juce::String& name, CardScope& scope);

struct Card
{
    juce::String id;        // stable and unique in its scope: "suite.rule.no-os-files"
    juce::String kind;      // "rule", "process", "tool", "knowledge", "personality" (personality cards always apply)
    juce::String title;
    juce::String text;      // the complete guidance, in words the model follows
    juce::StringArray tokens; // words that should bring this card up
    int priority = 50;      // 0..100, higher first
    juce::String status { "active" }; // "active" or "retired" (a retired card never applies, and hides the same id farther out)

    // Process cards: how work moves (all optional).
    juce::String trigger;       // when it applies
    juce::String authority;     // why it is binding
    juce::StringArray steps;    // in order
    juce::StringArray gates;    // what must not happen without an explicit override
    juce::StringArray evidence; // what proves it was followed
    juce::StringArray escalation; // when to stop and ask

    juce::String source;        // who or what made it: "user", "agent", a source file for shipped cards

    bool isActive() const { return status.isEmpty() || status.equalsIgnoreCase("active"); }
    bool operator==(const Card& other) const;
};

juce::var toVar(const Card& card);
Card cardFromVar(const juce::var& value);

// A card set as stored: {"cards": [ ... ]}.
juce::var cardsToVar(const juce::Array<Card>& cards);
juce::Array<Card> cardsFromVar(const juce::var& value);

// What is wrong with a card, in words a person can act on; empty if nothing. `others` are the rest of its set, for the
// unique-id check.
juce::String validateCard(const Card& card, const juce::Array<Card>& others);

// The kinds the Cards panel offers.
juce::StringArray cardKinds();
} // namespace creation::litesemrag
