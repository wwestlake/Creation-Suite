#include "creation/assistant/ConversationLedger.h"

#include <algorithm>

namespace creation::assistant
{
namespace
{
// An identifier becomes a file name, so it may only hold what a UUID holds.
bool safeId(const std::string& id)
{
    if (id.empty() || id.size() > 64)
        return false;
    for (char c : id)
        if (! ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-'))
            return false;
    return true;
}

std::string fileFor(const std::string& folder, const std::string& id) { return folder + id + ".json"; }

std::string idFromPath(const std::string& path)
{
    const auto slash = path.find_last_of('/');
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    const std::string suffix = ".json";
    if (name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
        name.resize(name.size() - suffix.size());
    return name;
}
}

std::vector<ConversationSummary> ConversationStore::listIn(const std::string& folder, bool archived)
{
    std::vector<ConversationSummary> found;
    for (const auto& path : files.list(folder))
    {
        const auto id = idFromPath(path);
        if (! safeId(id) || path != fileFor(folder, id))
            continue;

        ConversationSummary summary;
        summary.id = id;
        summary.archived = archived;

        std::string text, error;
        Conversation conversation;
        if (files.read(path, text) && ConversationLedger::fromJson(text, conversation, error)
            && conversation.id == id && ConversationLedger::verify(conversation, error))
        {
            summary.title = conversation.title;
            summary.updatedAt = conversation.updatedAt;
            summary.blockCount = (int) conversation.blocks.size();
            summary.intact = true;
        }
        else
        {
            // Unreadable or failed verification: still listed so it can be seen and removed, never loaded.
            summary.title = "[ALTERED] " + id;
            summary.intact = false;
        }
        found.push_back(std::move(summary));
    }
    std::sort(found.begin(), found.end(), [](const ConversationSummary& a, const ConversationSummary& b) {
        if (a.intact != b.intact)
            return a.intact;   // damaged ones last
        return a.updatedAt > b.updatedAt;
    });
    return found;
}

std::vector<ConversationSummary> ConversationStore::list() { return listIn(area.conversations(), false); }
std::vector<ConversationSummary> ConversationStore::listArchived() { return listIn(area.archive(), true); }

bool ConversationStore::loadFrom(const std::string& folder, const std::string& id, Conversation& conversation, std::string& error)
{
    if (! safeId(id))
    {
        error = "That is not a conversation identifier.";
        return false;
    }
    std::string text;
    if (! files.read(fileFor(folder, id), text))
    {
        error = "The conversation could not be read.";
        return false;
    }
    Conversation read;
    if (! ConversationLedger::fromJson(text, read, error))
        return false;
    if (read.id != id)
    {
        error = "The file holds a different conversation than its name says.";
        return false;
    }
    if (! ConversationLedger::verify(read, error))
        return false;
    conversation = std::move(read);
    return true;
}

bool ConversationStore::load(const std::string& id, Conversation& conversation, std::string& error)
{
    return loadFrom(area.conversations(), id, conversation, error);
}

bool ConversationStore::loadArchived(const std::string& id, Conversation& conversation, std::string& error)
{
    return loadFrom(area.archive(), id, conversation, error);
}

bool ConversationStore::save(const Conversation& conversation, std::string& error)
{
    if (! safeId(conversation.id))
    {
        error = "That is not a conversation identifier.";
        return false;
    }
    if (! ConversationLedger::verify(conversation, error))
        return false;
    if (! files.write(fileFor(area.conversations(), conversation.id), ConversationLedger::toJson(conversation)))
    {
        error = "The conversation could not be saved.";
        return false;
    }
    return true;
}

bool ConversationStore::move(const std::string& from, const std::string& to, const std::string& id, std::string& error)
{
    Conversation conversation;
    if (! loadFrom(from, id, conversation, error))   // never move something that fails verification
        return false;

    std::string original;
    if (! files.read(fileFor(from, id), original))
    {
        error = "The conversation could not be read.";
        return false;
    }
    // The unchanged bytes go to the new place, then the old copy is removed.
    if (! files.write(fileFor(to, id), original))
    {
        error = "The conversation could not be written to its new place.";
        return false;
    }
    if (! files.remove(fileFor(from, id)))
    {
        error = "The conversation was copied but the original could not be removed.";
        return false;
    }
    return true;
}

bool ConversationStore::archive(const std::string& id, std::string& error) { return move(area.conversations(), area.archive(), id, error); }
bool ConversationStore::unarchive(const std::string& id, std::string& error) { return move(area.archive(), area.conversations(), id, error); }

bool ConversationStore::remove(const std::string& id, std::string& error)
{
    if (! safeId(id))
    {
        error = "That is not a conversation identifier.";
        return false;
    }
    bool removed = false;
    for (const auto& folder : { area.conversations(), area.archive() })
    {
        const auto path = fileFor(folder, id);
        if (files.exists(path))
        {
            if (! files.remove(path))
            {
                error = "The conversation could not be removed.";
                return false;
            }
            removed = true;
        }
    }
    if (! removed)
        error = "There is no such conversation.";
    return removed;
}
}
