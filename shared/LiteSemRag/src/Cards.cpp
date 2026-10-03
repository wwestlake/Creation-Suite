#include <creation/litesemrag/Cards.h>

namespace creation::litesemrag
{
namespace
{
juce::var listToVar(const juce::StringArray& items)
{
    juce::Array<juce::var> values;
    for (const auto& item : items)
        values.add(item);
    return juce::var(values);
}

juce::StringArray listFromVar(const juce::var& value)
{
    juce::StringArray items;
    if (const auto* array = value.getArray())
        for (const auto& item : *array)
            if (item.toString().trim().isNotEmpty())
                items.add(item.toString().trim());
    return items;
}
} // namespace

juce::String scopeName(CardScope scope)
{
    switch (scope)
    {
        case CardScope::shipped: return "shipped";
        case CardScope::suite: return "suite";
        case CardScope::app: return "app";
        case CardScope::project: return "project";
    }
    return {};
}

bool scopeFromName(const juce::String& name, CardScope& scope)
{
    for (auto candidate : { CardScope::shipped, CardScope::suite, CardScope::app, CardScope::project })
        if (scopeName(candidate).equalsIgnoreCase(name.trim()))
        {
            scope = candidate;
            return true;
        }
    return false;
}

bool Card::operator==(const Card& other) const
{
    return id == other.id && kind == other.kind && title == other.title && text == other.text && tokens == other.tokens
        && priority == other.priority && status == other.status && trigger == other.trigger && authority == other.authority
        && steps == other.steps && gates == other.gates && evidence == other.evidence && escalation == other.escalation
        && source == other.source;
}

juce::var toVar(const Card& card)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("id", card.id);
    object->setProperty("kind", card.kind);
    object->setProperty("title", card.title);
    object->setProperty("text", card.text);
    object->setProperty("tokens", listToVar(card.tokens));
    object->setProperty("priority", card.priority);
    object->setProperty("status", card.status);
    // Process fields only when used, so a plain rule card stays small.
    if (card.trigger.isNotEmpty()) object->setProperty("trigger", card.trigger);
    if (card.authority.isNotEmpty()) object->setProperty("authority", card.authority);
    if (! card.steps.isEmpty()) object->setProperty("steps", listToVar(card.steps));
    if (! card.gates.isEmpty()) object->setProperty("gates", listToVar(card.gates));
    if (! card.evidence.isEmpty()) object->setProperty("evidence", listToVar(card.evidence));
    if (! card.escalation.isEmpty()) object->setProperty("escalation", listToVar(card.escalation));
    if (card.source.isNotEmpty()) object->setProperty("source", card.source);
    return juce::var(object);
}

Card cardFromVar(const juce::var& value)
{
    Card card;
    card.id = value.getProperty("id", {}).toString().trim();
    card.kind = value.getProperty("kind", {}).toString().trim();
    card.title = value.getProperty("title", {}).toString();
    card.text = value.getProperty("text", {}).toString();
    card.tokens = listFromVar(value.getProperty("tokens", {}));
    card.priority = static_cast<int>(value.getProperty("priority", 50));
    card.status = value.getProperty("status", "active").toString();
    card.trigger = value.getProperty("trigger", {}).toString();
    card.authority = value.getProperty("authority", {}).toString();
    card.steps = listFromVar(value.getProperty("steps", {}));
    card.gates = listFromVar(value.getProperty("gates", {}));
    card.evidence = listFromVar(value.getProperty("evidence", {}));
    card.escalation = listFromVar(value.getProperty("escalation", {}));
    card.source = value.getProperty("source", {}).toString();
    return card;
}

juce::var cardsToVar(const juce::Array<Card>& cards)
{
    juce::Array<juce::var> values;
    for (const auto& card : cards)
        values.add(toVar(card));
    auto* object = new juce::DynamicObject();
    object->setProperty("cards", values);
    return juce::var(object);
}

juce::Array<Card> cardsFromVar(const juce::var& value)
{
    juce::Array<Card> cards;
    if (const auto* array = value.getProperty("cards", {}).getArray())
        for (const auto& item : *array)
            if (item.isObject())
                cards.add(cardFromVar(item));
    return cards;
}

juce::String validateCard(const Card& card, const juce::Array<Card>& others)
{
    if (card.id.isEmpty())
        return "A card needs an id.";
    if (card.id.containsAnyOf(" \t\r\n"))
        return "A card's id has no spaces - use dots or dashes (\"suite.rule.no-os-files\").";
    for (const auto& other : others)
        if (other.id == card.id && &other != &card)
            return "There is already a card with the id \"" + card.id + "\".";
    if (card.title.trim().isEmpty())
        return "A card needs a title.";
    if (card.text.trim().isEmpty())
        return "A card needs its text - the guidance itself.";
    if (card.priority < 0 || card.priority > 100)
        return "A card's priority is from 0 to 100.";
    if (! card.status.equalsIgnoreCase("active") && ! card.status.equalsIgnoreCase("retired"))
        return "A card is active or retired.";
    return {};
}

juce::StringArray cardKinds()
{
    return { "rule", "process", "tool", "knowledge", "personality" };
}
} // namespace creation::litesemrag
