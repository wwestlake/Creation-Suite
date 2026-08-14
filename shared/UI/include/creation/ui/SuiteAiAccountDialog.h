#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <creation/services/SuiteAiSettings.h>
#include <functional>

namespace creation::ui
{
// The "Add Account" / "Edit Account" popup for the Suite AI Accounts tab. Pick a provider, enter
// a key, press Connect to call that provider's real model-list API and cache the result on the
// account (so every app/tool reads the same list instead of re-querying) - the account is only
// committed to the caller's settings when Save is pressed. No model is picked here - which model
// to use is chosen at the tool/app level from the account's cached list, not baked into the
// account itself.
class SuiteAiAccountDialog final : public juce::Component
{
public:
    SuiteAiAccountDialog(juce::Array<creation::services::SuiteAiProviderPreset> providers,
                         creation::services::SuiteAiAccountSettings existingAccount);

    // Fired once, either with the saved account or not at all (Cancel just closes the dialog).
    std::function<void(const creation::services::SuiteAiAccountSettings&)> onSaved;
    std::function<void()> onCancelled;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void connectAndCacheModels();
    void save();

    juce::Label providerLabel { {}, "Provider" };
    juce::ComboBox providerCombo;
    juce::Label nameLabel { {}, "Account Name" };
    juce::TextEditor nameEditor;
    juce::Label keyLabel { {}, "API Key / Token" };
    juce::TextEditor keyEditor;
    juce::TextButton connectButton { "Connect" };
    juce::Label modelStatusLabel;
    juce::TextButton saveButton { "Save" };
    juce::TextButton cancelButton { "Cancel" };

    juce::Array<creation::services::SuiteAiProviderPreset> providers;
    creation::services::SuiteAiAccountSettings account;
    bool isNewAccount = false;
};

// Shows the dialog in its own DialogWindow. onComplete fires with (true, account) on Save, or
// (false, {}) on Cancel/close. `existingAccount` with an empty accountId means "new account".
void showAddOrEditAiAccountDialog(const juce::Array<creation::services::SuiteAiProviderPreset>& providers,
                                  const creation::services::SuiteAiAccountSettings& existingAccount,
                                  std::function<void(bool saved, const creation::services::SuiteAiAccountSettings&)> onComplete);
}
