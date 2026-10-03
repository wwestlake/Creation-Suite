#include <creation/litesemrag/Embeddings.h>

#include <creation/services/SuiteVfsJsonStore.h>

#include <algorithm>
#include <cmath>

namespace creation::litesemrag
{
namespace
{
juce::String entryFor(const juce::String& model)
{
    return "cards/embeddings/" + model.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.") + ".json";
}
} // namespace

juce::String embeddingText(const Card& card)
{
    juce::String text;
    text << card.title.trim();
    if (card.kind.isNotEmpty())
        text << " (" << card.kind << ")";
    text << "\n";
    if (card.trigger.isNotEmpty())
        text << "When: " << card.trigger.trim() << "\n";
    if (! card.tokens.isEmpty())
        text << "About: " << card.tokens.joinIntoString(", ") << "\n";
    text << card.text.trim();
    return text;
}

juce::String embeddingKey(const Card& card, const juce::String& model)
{
    const auto text = embeddingText(card);
    // Two independent 64-bit hashes of the words, and their length: a collision would need all three to agree.
    const auto first = text.hashCode64();
    const auto second = (model + "\n" + text + "\n" + model).hashCode64();
    return juce::String::toHexString(first) + juce::String::toHexString(second) + juce::String::toHexString(text.length());
}

float similarity(const std::vector<float>& a, const std::vector<float>& b)
{
    if (a.empty() || a.size() != b.size())
        return 0.0f;
    double dot = 0.0, lengthA = 0.0, lengthB = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
    {
        dot += static_cast<double>(a[i]) * b[i];
        lengthA += static_cast<double>(a[i]) * a[i];
        lengthB += static_cast<double>(b[i]) * b[i];
    }
    if (lengthA <= 0.0 || lengthB <= 0.0)
        return 0.0f;
    return static_cast<float>(dot / (std::sqrt(lengthA) * std::sqrt(lengthB)));
}

EmbeddingCache EmbeddingCache::load(const juce::String& model, juce::String& errorMessage)
{
    EmbeddingCache cache;
    cache.model = model;
    const auto stored = creation::services::SuiteVfsJsonStore::loadJson(entryFor(model), errorMessage);
    if (const auto* list = stored.getProperty("embeddings", {}).getArray())
        for (const auto& item : *list)
        {
            const auto key = item.getProperty("key", {}).toString();
            const auto* numbers = item.getProperty("vector", {}).getArray();
            if (key.isEmpty() || numbers == nullptr || numbers->isEmpty())
                continue;
            Entry entry;
            entry.madeAt = static_cast<juce::int64>(item.getProperty("madeAt", 0));
            entry.vector.reserve(static_cast<size_t>(numbers->size()));
            for (const auto& number : *numbers)
                entry.vector.push_back(static_cast<float>(static_cast<double>(number)));
            cache.entries[key] = std::move(entry);
        }
    return cache;
}

const std::vector<float>* EmbeddingCache::find(const juce::String& key) const
{
    const auto found = entries.find(key);
    return found != entries.end() ? &found->second.vector : nullptr;
}

void EmbeddingCache::put(const juce::String& key, std::vector<float> vector)
{
    entries[key] = { std::move(vector), juce::Time::currentTimeMillis() };
    changed = true;
}

bool EmbeddingCache::save(juce::String& errorMessage)
{
    if (! changed)
        return true;

    std::vector<std::pair<juce::String, const Entry*>> kept;
    for (const auto& [key, entry] : entries)
        kept.push_back({ key, &entry });
    std::stable_sort(kept.begin(), kept.end(), [](const auto& a, const auto& b) { return a.second->madeAt > b.second->madeAt; });
    if (kept.size() > static_cast<size_t>(maxEntries))
        kept.resize(static_cast<size_t>(maxEntries));

    juce::Array<juce::var> list;
    for (const auto& [key, entry] : kept)
    {
        juce::Array<juce::var> numbers;
        numbers.ensureStorageAllocated(static_cast<int>(entry->vector.size()));
        for (const auto number : entry->vector)
            numbers.add(std::round(static_cast<double>(number) * 1.0e6) / 1.0e6);
        auto* item = new juce::DynamicObject();
        item->setProperty("key", key);
        item->setProperty("madeAt", entry->madeAt);
        item->setProperty("vector", juce::var(numbers));
        list.add(juce::var(item));
    }
    auto* root = new juce::DynamicObject();
    root->setProperty("model", model);
    root->setProperty("embeddings", juce::var(list));
    if (! creation::services::SuiteVfsJsonStore::saveJson(entryFor(model), juce::var(root), errorMessage))
        return false;
    changed = false;
    return true;
}
} // namespace creation::litesemrag
