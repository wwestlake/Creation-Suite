#include <creation/ui/SuiteAiAccountDialog.h>
#include <creation/services/SuiteAiModelCatalogClient.h>
#include <creation/services/SuiteAiProviderRuntime.h>

namespace
{
void styleEditor(juce::TextEditor& editor)
{
    editor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1a2230));
    editor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2a3a50));
    editor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
}

void styleLabel(juce::Label& label)
{
    label.setColour(juce::Label::textColourId, juce::Colour(0xffdce6f5));
}

void styleCombo(juce::ComboBox& comboBox)
{
    comboBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1a2230));
    comboBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff2a3a50));
    comboBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
}
}

namespace creation::ui
{
SuiteAiAccountDialog::SuiteAiAccountDialog(juce::Array<creation::services::SuiteAiProviderPreset> providersIn,
                                           creation::services::SuiteAiAccountSettings existingAccount)
    : providers(std::move(providersIn)), account(std::move(existingAccount))
{
    isNewAccount = account.accountId.trim().isEmpty();

    styleLabel(providerLabel);
    addAndMakeVisible(providerLabel);
    styleCombo(providerCombo);
    for (int index = 0; index < providers.size(); ++index)
        providerCombo.addItem(providers[index].displayName, index + 1);
    if (providers.size() > 0)
    {
        const auto wantedProviderId = account.providerId.isNotEmpty() ? account.providerId : providers[0].id;
        auto selectedIndex = 0;
        for (int index = 0; index < providers.size(); ++index)
            if (providers[index].id == wantedProviderId)
                selectedIndex = index;
        providerCombo.setSelectedItemIndex(selectedIndex, juce::dontSendNotification);
    }
    addAndMakeVisible(providerCombo);

    styleLabel(nameLabel);
    addAndMakeVisible(nameLabel);
    styleEditor(nameEditor);
    nameEditor.setText(account.accountLabel, juce::dontSendNotification);
    nameEditor.setTextToShowWhenEmpty("e.g. Work OpenAI", juce::Colour(0xff6a7d99));
    addAndMakeVisible(nameEditor);

    styleLabel(keyLabel);
    addAndMakeVisible(keyLabel);
    styleEditor(keyEditor);
    keyEditor.setText(account.apiKey, juce::dontSendNotification);
    keyEditor.setPasswordCharacter(0x2022);
    addAndMakeVisible(keyEditor);

    refreshButton.onClick = [this] { refreshModels(); };
    addAndMakeVisible(refreshButton);

    modelStatusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff97a9c1));
    modelStatusLabel.setText(account.cachedModelIds.isEmpty()
                                  ? "Press Refresh to fetch this account's available models."
                                  : juce::String(account.cachedModelIds.size()) + " model(s) cached.",
                             juce::dontSendNotification);
    addAndMakeVisible(modelStatusLabel);

    styleLabel(modelLabel);
    addAndMakeVisible(modelLabel);
    styleCombo(modelCombo);
    modelCombo.setTextWhenNoChoicesAvailable("No models fetched yet");
    updateModelCombo();
    addAndMakeVisible(modelCombo);

    saveButton.onClick = [this] { save(); };
    addAndMakeVisible(saveButton);

    cancelButton.onClick = [this] { if (onCancelled) onCancelled(); };
    addAndMakeVisible(cancelButton);

    setSize(420, 320);
}

void SuiteAiAccountDialog::updateModelCombo()
{
    auto previousText = modelCombo.getText();
    modelCombo.clear(juce::dontSendNotification);
    for (int index = 0; index < account.cachedModelIds.size(); ++index)
        modelCombo.addItem(account.cachedModelIds[index], index + 1);

    if (account.modelName.isNotEmpty())
        modelCombo.setText(account.modelName, juce::dontSendNotification);
    else if (previousText.isNotEmpty())
        modelCombo.setText(previousText, juce::dontSendNotification);
    else if (! account.cachedModelIds.isEmpty())
        modelCombo.setSelectedItemIndex(0, juce::dontSendNotification);
}

void SuiteAiAccountDialog::refreshModels()
{
    const auto providerIndex = providerCombo.getSelectedItemIndex();
    if (! juce::isPositiveAndBelow(providerIndex, providers.size()))
        return;

    const auto& provider = providers.getReference(providerIndex);
    const auto apiKey = keyEditor.getText().trim();

    modelStatusLabel.setText("Fetching models...", juce::dontSendNotification);

    creation::services::SuiteAiModelCatalogClient client;
    juce::StringArray modelIds;
    juce::String errorMessage;
    if (! client.fetchModelIds(provider.defaultBaseUrl, provider.id, apiKey, modelIds, errorMessage))
    {
        modelStatusLabel.setText(errorMessage.isNotEmpty() ? errorMessage : "Could not fetch models.",
                                 juce::dontSendNotification);
        return;
    }

    account.cachedModelIds = modelIds;
    account.modelsFetchedAt = juce::Time::getCurrentTime().toISO8601(true);
    modelStatusLabel.setText(juce::String(modelIds.size()) + " model(s) found.", juce::dontSendNotification);
    updateModelCombo();
}

void SuiteAiAccountDialog::save()
{
    const auto providerIndex = providerCombo.getSelectedItemIndex();
    if (juce::isPositiveAndBelow(providerIndex, providers.size()))
    {
        const auto& provider = providers.getReference(providerIndex);
        account.providerId = provider.id;
        if (account.baseUrl.trim().isEmpty())
            account.baseUrl = provider.defaultBaseUrl;
    }

    if (isNewAccount)
        account.accountId = "suite-account-" + juce::Uuid().toString();

    account.accountLabel = nameEditor.getText().trim();
    account.apiKey = keyEditor.getText().trim();
    account.modelName = modelCombo.getText().trim();
    account.enabled = true;

    if (onSaved)
        onSaved(account);
}

void SuiteAiAccountDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff182131));
}

void SuiteAiAccountDialog::resized()
{
    auto area = getLocalBounds().reduced(16);

    auto row = [&area](int height)
    {
        return area.removeFromTop(height);
    };

    providerLabel.setBounds(row(18));
    providerCombo.setBounds(row(28));
    area.removeFromTop(8);

    nameLabel.setBounds(row(18));
    nameEditor.setBounds(row(28));
    area.removeFromTop(8);

    keyLabel.setBounds(row(18));
    keyEditor.setBounds(row(28));
    area.removeFromTop(8);

    auto refreshRow = row(28);
    refreshButton.setBounds(refreshRow.removeFromLeft(90));
    refreshRow.removeFromLeft(8);
    modelStatusLabel.setBounds(refreshRow);
    area.removeFromTop(8);

    modelLabel.setBounds(row(18));
    modelCombo.setBounds(row(28));
    area.removeFromTop(16);

    auto buttonsRow = row(30);
    cancelButton.setBounds(buttonsRow.removeFromRight(90));
    buttonsRow.removeFromRight(8);
    saveButton.setBounds(buttonsRow.removeFromRight(90));
}

void showAddOrEditAiAccountDialog(const juce::Array<creation::services::SuiteAiProviderPreset>& providers,
                                  const creation::services::SuiteAiAccountSettings& existingAccount,
                                  std::function<void(bool saved, const creation::services::SuiteAiAccountSettings&)> onComplete)
{
    auto* content = new SuiteAiAccountDialog(providers, existingAccount);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(content);
    options.dialogTitle = existingAccount.accountId.trim().isEmpty() ? "Add AI Account" : "Edit AI Account";
    options.dialogBackgroundColour = juce::Colour(0xff182131);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = false;

    auto* window = options.launchAsync();
    auto safeWindow = juce::Component::SafePointer<juce::DialogWindow>(window);

    content->onSaved = [safeWindow, onComplete](const creation::services::SuiteAiAccountSettings& account)
    {
        if (onComplete)
            onComplete(true, account);
        if (safeWindow != nullptr)
            safeWindow->exitModalState(0);
    };

    content->onCancelled = [safeWindow, onComplete]
    {
        if (onComplete)
            onComplete(false, {});
        if (safeWindow != nullptr)
            safeWindow->exitModalState(0);
    };
}
}
