#pragma once

#include <creation/agent/VirtualEngineer.h>
#include <creation/ui/SuiteAiChatPanel.h>

namespace creation::agent
{
// The suite's chat panel bound to an app's Virtual Engineer: what is typed goes to the engineer, its reply comes back
// into the transcript with what it did (each action) and the cards it used. Above it: Stop while a request runs, and
// Undo for the last request that changed something. A destructive or external action asks here first - also when the
// request came through the engineer's API. Choosing an account or model in the panel picks the suite account for this
// app in the suite's AI settings - accounts themselves are only made in the suite's settings.
class EngineerChat final : public juce::Component, private juce::Timer
{
public:
    explicit EngineerChat(VirtualEngineer& engineer);
    ~EngineerChat() override;

    creation::ui::SuiteAiChatPanel& getPanel() noexcept { return panel; }
    // Re-reads the suite's accounts (after the suite's settings changed).
    void refreshAccounts();

    void paint(juce::Graphics& g) override;
    void resized() override;

    // The reply as the transcript shows it: the text, then each action and the cards used.
    static juce::String describe(const AskResult& result);

private:
    void timerCallback() override;
    void saveSelection(const juce::String& accountId, const juce::String& model);
    void undo();

    VirtualEngineer& engineer;
    creation::ui::SuiteAiChatPanel panel;
    juce::Label state;
    juce::TextButton stopButton { "Stop" }, undoButton { "Undo last request" };
};
} // namespace creation::agent
