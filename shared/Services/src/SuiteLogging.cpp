#include <creation/services/SuiteLogging.h>
#include <creation/suite/SuiteSettings.h>

namespace
{
juce::File getSuiteConfigDirectory()
{
    return creation::suite::SuiteSettingsStore().getSuiteConfigDirectory();
}

juce::var toVar(const creation::services::SuiteLogEntry& entry)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("timestampMs", entry.timestamp.toMilliseconds());
    object->setProperty("subsystem", entry.subsystem);
    object->setProperty("category", entry.category);
    object->setProperty("message", entry.message);
    object->setProperty("details", entry.details);
    object->setProperty("correlationId", entry.correlationId);
    object->setProperty("appDomain", creation::assets::toStorageToken(entry.appDomain));
    object->setProperty("level", static_cast<int>(entry.level));
    return juce::var(object);
}

creation::services::SuiteLogEntry fromVar(const juce::var& value)
{
    creation::services::SuiteLogEntry entry;
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return entry;

    entry.timestamp = juce::Time(static_cast<juce::int64>(object->getProperty("timestampMs")));
    entry.subsystem = object->getProperty("subsystem").toString();
    entry.category = object->getProperty("category").toString();
    entry.message = object->getProperty("message").toString();
    entry.details = object->getProperty("details").toString();
    entry.correlationId = object->getProperty("correlationId").toString();
    entry.appDomain = creation::assets::suiteAppDomainFromStorageToken(object->getProperty("appDomain").toString());
    entry.level = static_cast<creation::services::SuiteLogLevel>(static_cast<int>(object->getProperty("level")));
    return entry;
}
}

namespace creation::services
{
juce::Array<SuiteLogEntry> SuiteLogStore::load(juce::String& errorMessage) const
{
    juce::Array<SuiteLogEntry> entries;
    auto settingsFile = getSettingsFile();
    if (! settingsFile.existsAsFile())
        return entries;

    const auto parsed = juce::JSON::parse(settingsFile);
    if (parsed.isVoid())
    {
        errorMessage = "Could not parse the suite log file.";
        return {};
    }

    if (const auto* object = parsed.getDynamicObject())
        if (const auto* array = object->getProperty("entries").getArray())
            for (const auto& value : *array)
                entries.add(fromVar(value));

    return entries;
}

juce::Array<SuiteLogEntry> SuiteLogStore::loadRecent(int maxEntries, juce::String& errorMessage) const
{
    auto entries = load(errorMessage);
    if (errorMessage.isNotEmpty() || maxEntries <= 0 || entries.size() <= maxEntries)
        return entries;

    juce::Array<SuiteLogEntry> recent;
    for (int index = entries.size() - maxEntries; index < entries.size(); ++index)
        recent.add(entries.getReference(index));
    return recent;
}

bool SuiteLogStore::save(const juce::Array<SuiteLogEntry>& entries,
                         juce::String& errorMessage) const
{
    auto configDirectory = getSuiteConfigDirectory();
    if (! configDirectory.exists() && ! configDirectory.createDirectory())
    {
        errorMessage = "Could not create the suite configuration folder for logging.";
        return false;
    }

    juce::Array<juce::var> values;
    for (const auto& entry : entries)
        values.add(toVar(entry));

    auto* object = new juce::DynamicObject();
    object->setProperty("entries", values);
    if (! getSettingsFile().replaceWithText(juce::JSON::toString(juce::var(object), true)))
    {
        errorMessage = "Could not save the suite log file.";
        return false;
    }

    return true;
}

bool SuiteLogStore::append(const SuiteLogEntry& entry,
                           juce::String& errorMessage,
                           int maxEntriesToKeep) const
{
    auto entries = load(errorMessage);
    if (errorMessage.isNotEmpty())
        return false;

    entries.add(entry);
    while (maxEntriesToKeep > 0 && entries.size() > maxEntriesToKeep)
        entries.remove(0);

    return save(entries, errorMessage);
}

juce::File SuiteLogStore::getSettingsFile() const
{
    return getSuiteConfigDirectory().getChildFile("suite-activity-log.json");
}

SuiteLogEntry SuiteLogger::makeEntry(const juce::String& subsystem,
                                     const juce::String& category,
                                     const juce::String& message,
                                     SuiteLogLevel level,
                                     creation::assets::SuiteAppDomain appDomain,
                                     const juce::String& details,
                                     const juce::String& correlationId)
{
    SuiteLogEntry entry;
    entry.timestamp = juce::Time::getCurrentTime();
    entry.subsystem = subsystem;
    entry.category = category;
    entry.message = message;
    entry.level = level;
    entry.appDomain = appDomain;
    entry.details = details;
    entry.correlationId = correlationId;
    return entry;
}

bool SuiteLogger::log(const SuiteLogEntry& entry,
                      juce::String& errorMessage,
                      int maxEntriesToKeep)
{
    return SuiteLogStore().append(entry, errorMessage, maxEntriesToKeep);
}
}
