#pragma once

#include <creation/agent/VirtualEngineer.h>
#include <creation/ui/SuiteAiChatPanel.h>

namespace creation::agent
{
// The suite's chat panel bound to an app's Virtual Engineer: what is typed goes to the engineer, its reply (with the
// cards it used) comes back into the transcript. Choosing an account or model in the panel picks the suite account
// for this app in the suite's AI settings - accounts themselves are only made in the suite's settings.
class EngineerChat final : public juce::Component
{
public:
    explicit EngineerChat(VirtualEngineer& engineer);

    creation::ui::SuiteAiChatPanel& getPanel() noexcept { return panel; }
    // Re-reads the suite's accounts (after the suite's settings changed).
    void refreshAccounts();

    void resized() override;

private:
    void saveSelection(const juce::String& accountId, const juce::String& model);

    VirtualEngineer& engineer;
    creation::ui::SuiteAiChatPanel panel;
};
} // namespace creation::agent
