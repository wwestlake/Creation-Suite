#include <creation/agent/EngineerChat.h>

namespace creation::agent
{
namespace services = creation::services;

EngineerChat::EngineerChat(VirtualEngineer& e) : engineer(e)
{
    addAndMakeVisible(panel);
    panel.onPromptSubmitted = [this](const juce::String& prompt) {
        const bool sent = engineer.ask(prompt, [safe = juce::Component::SafePointer<EngineerChat>(this)](const AskResult& result) {
            if (safe == nullptr)
                return;
            if (! result.ok)
            {
                safe->panel.setAssistantResponse(result.error);
                return;
            }
            // Which cards guided the answer - so writing a card shows whether it is picked up.
            juce::StringArray used;
            if (const auto* cards = result.details.getProperty("cards", {}).getArray())
                for (const auto& card : *cards)
                    used.add(card.getProperty("title", {}).toString());
            safe->panel.setAssistantResponse(result.text + (used.isEmpty() ? juce::String() : "\n\n(Cards: " + used.joinIntoString(", ") + ")"));
        });
        if (! sent)
            panel.setAssistantResponse("Still answering the last request - send again when it has answered.");
    };
    panel.onAccountChanged = [this](const juce::String& accountId) { saveSelection(accountId, {}); };
    panel.onModelChanged = [this](const juce::String& model) {
        juce::String error;
        const auto settings = services::SuiteAiSettingsStore().load(error);
        saveSelection(services::SuiteAiSettingsResolver::resolveRuntimeSettingsForApp(settings, engineer.getApp()).accountId, model);
    };
    refreshAccounts();
}

void EngineerChat::refreshAccounts()
{
    juce::String error;
    const auto settings = services::SuiteAiSettingsStore().load(error);
    const auto runtime = services::SuiteAiSettingsResolver::resolveRuntimeSettingsForApp(settings, engineer.getApp());
    panel.RefreshConfiguredAccounts();
    panel.setSelectedAccountId(runtime.accountId);
    panel.setSelectedModel(runtime.modelName);
}

void EngineerChat::saveSelection(const juce::String& accountId, const juce::String& model)
{
    juce::String error;
    auto settings = services::SuiteAiSettingsStore().load(error);
    if (error.isNotEmpty() || accountId.isEmpty())
        return;
    services::SuiteAiSettingsResolver::selectAccountForApp(settings, engineer.getApp(), accountId, model);
    services::SuiteAiSettingsStore().save(settings, error);
}

void EngineerChat::resized()
{
    panel.setBounds(getLocalBounds());
}
} // namespace creation::agent
