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
            safe->panel.setAssistantResponse(result.ok ? describe(result) : result.error);
            safe->timerCallback();
        });
        if (! sent)
            panel.setAssistantResponse("Still working on the last request - send again when it has answered, or press Stop.");
        timerCallback();
    };
    panel.onAccountChanged = [this](const juce::String& accountId) { saveSelection(accountId, {}); };
    panel.onModelChanged = [this](const juce::String& model) {
        juce::String error;
        const auto settings = services::SuiteAiSettingsStore().load(error);
        saveSelection(services::SuiteAiSettingsResolver::resolveRuntimeSettingsForApp(settings, engineer.getApp()).accountId, model);
    };

    state.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(state);
    stopButton.onClick = [this] {
        engineer.cancel();
        timerCallback();
    };
    addAndMakeVisible(stopButton);
    undoButton.onClick = [this] { undo(); };
    addAndMakeVisible(undoButton);

    // Destructive and external actions ask the user, here, whoever sent the request.
    engineer.approver = [safe = juce::Component::SafePointer<EngineerChat>(this)](const ApprovalRequest& request, std::function<void(bool)> decide) {
        if (safe == nullptr)
            return decide(false);
        juce::String message;
        message << "The Virtual Engineer wants to: " << request.toolTitle << "\n"
                << request.description << "\n\n"
                << "With: " << request.arguments << "\n\n"
                << "For the request: \"" << request.request << "\"";
        if (request.effect == Effect::external)
            message << "\n\nThis reaches outside the open work and cannot be undone with the request.";
        else
            message << "\n\nUndo last request will still restore it.";
        juce::AlertWindow::showAsync(juce::MessageBoxOptions()
                                         .withIconType(juce::MessageBoxIconType::QuestionIcon)
                                         .withTitle("Allow this action?")
                                         .withMessage(message)
                                         .withButton("Allow")
                                         .withButton("Don't allow")
                                         .withAssociatedComponent(safe.getComponent()),
                                     [decide](int button) { decide(button == 1); });
    };

    refreshAccounts();
    timerCallback();
    startTimer(250);
}

EngineerChat::~EngineerChat()
{
    engineer.approver = nullptr;
}

juce::String EngineerChat::describe(const AskResult& result)
{
    juce::String text = result.text;
    if (const auto* actions = result.details.getProperty("actions", {}).getArray(); actions != nullptr && ! actions->isEmpty())
    {
        text << "\n\nActions:";
        for (const auto& action : *actions)
        {
            const auto outcome = action.getProperty("result", {});
            const auto title = action.getProperty("title", {}).toString();
            if (static_cast<bool>(outcome.getProperty("ok", false)))
            {
                juce::StringArray changes;
                if (const auto* list = outcome.getProperty("changes", {}).getArray())
                    for (const auto& change : *list)
                        changes.add(change.toString());
                text << "\n  - " << title << (changes.isEmpty() ? juce::String() : ": " + changes.joinIntoString("; "));
            }
            else
                text << "\n  x " << title << ": " << outcome.getProperty("error", {}).getProperty("message", {}).toString();
        }
        if (static_cast<bool>(result.details.getProperty("canUndo", false)))
            text << "\n(Undo last request puts everything back as it was before.)";
    }
    if (const auto unavailable = result.details.getProperty("toolsUnavailable", {}).toString(); unavailable.isNotEmpty())
        text << "\n\n(" << unavailable << ")";
    juce::StringArray used;
    if (const auto* cards = result.details.getProperty("cards", {}).getArray())
        for (const auto& card : *cards)
            used.add(card.getProperty("title", {}).toString());
    if (! used.isEmpty())
        text << "\n\n(Cards: " << used.joinIntoString(", ") << ")";
    return text;
}

void EngineerChat::timerCallback()
{
    const bool busy = engineer.isBusy();
    stopButton.setEnabled(busy);
    undoButton.setEnabled(! busy && engineer.canUndoLastRequest());
    undoButton.setTooltip(engineer.canUndoLastRequest() ? "Undo: " + engineer.lastRequestLabel() : juce::String());
    state.setText(busy ? "Working..." : juce::String(), juce::dontSendNotification);
}

void EngineerChat::undo()
{
    const auto label = engineer.lastRequestLabel();
    const auto outcome = engineer.undoLastRequest(false);
    if (outcome == VirtualEngineer::UndoOutcome::undone)
    {
        panel.setAssistantResponse("Undone: \"" + label + "\".");
        timerCallback();
        return;
    }
    if (outcome != VirtualEngineer::UndoOutcome::editedSince)
        return;
    juce::AlertWindow::showAsync(juce::MessageBoxOptions()
                                     .withIconType(juce::MessageBoxIconType::WarningIcon)
                                     .withTitle("Undo anyway?")
                                     .withMessage("The work changed after the request \"" + label + "\". Undoing it puts everything back as it "
                                                  "was before that request, and your later changes are lost.")
                                     .withButton("Undo anyway")
                                     .withButton("Keep")
                                     .withAssociatedComponent(this),
                                 [safe = juce::Component::SafePointer<EngineerChat>(this), label](int button) {
                                     if (safe == nullptr || button != 1)
                                         return;
                                     if (safe->engineer.undoLastRequest(true) == VirtualEngineer::UndoOutcome::undone)
                                         safe->panel.setAssistantResponse("Undone: \"" + label + "\".");
                                     safe->timerCallback();
                                 });
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

void EngineerChat::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1f22));
}

void EngineerChat::resized()
{
    auto area = getLocalBounds();
    auto bar = area.removeFromTop(30).reduced(6, 3);
    undoButton.setBounds(bar.removeFromRight(130));
    bar.removeFromRight(4);
    stopButton.setBounds(bar.removeFromRight(60));
    state.setBounds(bar);
    panel.setBounds(area);
}
} // namespace creation::agent
