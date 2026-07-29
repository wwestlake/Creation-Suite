#include <creation/ui/SuiteSettingsPanel.h>

namespace
{
juce::Colour panelFill() { return juce::Colour(0xff121822); }
juce::Colour panelOutline() { return juce::Colour(0xff2a3a50); }
juce::Colour labelColour() { return juce::Colour(0xffdce6f5); }
juce::Colour hintColour() { return juce::Colour(0xff97a9c1); }

int comboItemIdForIndex(int index)
{
    return index + 1;
}

void configureEditor(juce::TextEditor& editor)
{
    editor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1a2230));
    editor.setColour(juce::TextEditor::outlineColourId, panelOutline());
    editor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
}

void configureLabel(juce::Label& label)
{
    label.setColour(juce::Label::textColourId, labelColour());
}

void configureCombo(juce::ComboBox& comboBox)
{
    comboBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1a2230));
    comboBox.setColour(juce::ComboBox::outlineColourId, panelOutline());
    comboBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
}

}

SuiteSettingsPanel::SuiteSettingsPanel()
    : aiProviders(creation::services::SuiteAiProviderCatalog::createDefaultCatalog())
{
    titleLabel.setText("Creation Suite Control", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(24.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    subTitleLabel.setText("Manage the suite-wide VFS, app locations, shared AI accounts, and app-level AI routing.",
                          juce::dontSendNotification);
    subTitleLabel.setColour(juce::Label::textColourId, hintColour());
    addAndMakeVisible(subTitleLabel);

    storageSectionLabel.setText("Storage And VFS", juce::dontSendNotification);
    storageSectionLabel.setFont(juce::Font(18.0f).boldened());
    storageSectionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(storageSectionLabel);

    configureRow(suiteVfsRow, "suite_vfs_root", "Suite VFS Root");
    configureRow(sharedResourcesRow, "shared_resources_root", "Shared Resources");
    configureRow(projectContainersRow, "project_containers_root", "Project Containers");
    configureRow(cacheRootRow, "cache_root", "Cache Root");
    configureRow(materializedFilesRow, "materialized_files_root", "Materialized Files");
    configureRow(exportsRootRow, "exports_root", "Exports Root");
    configureRow(stationProjectsRow, "creation_station_projects_root", "Creation Station Projects");
    configureRow(engineProjectsRow, "creation_engine_projects_root", "Creation Engine Projects");
    configureRow(movieProjectsRow, "creation_movie_projects_root", "Creation Movie Projects");
    configureRow(liveProjectsRow, "creation_live_projects_root", "Creation Live Projects");

    aiSectionLabel.setText("Suite AI Accounts And Routing", juce::dontSendNotification);
    aiSectionLabel.setFont(juce::Font(18.0f).boldened());
    aiSectionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(aiSectionLabel);

    configureCombo(accountSelectorCombo);
    accountSelectorCombo.onChange = [this]
    {
        pushCurrentEditorToSelectedAccount();
        selectedAccountIndex = accountSelectorCombo.getSelectedItemIndex();
        pullSelectedAccountIntoEditor();
        refreshAiAccountUi();
    };
    addAndMakeVisible(accountSelectorCombo);

    addAccountButton.onClick = [this]
    {
        pushCurrentEditorToSelectedAccount();

        creation::services::SuiteAiAccountSettings account;
        account.accountId = "suite-account-" + juce::Uuid().toString();
        account.providerId = "openai";
        account.accountLabel = "New Suite Account";
        account.baseUrl = "https://api.openai.com/v1";
        account.modelName = "gpt-4.1-mini";
        account.enabled = true;
        aiSettings.accounts.add(account);

        selectedAccountIndex = aiSettings.accounts.size() - 1;
        refreshAiAccountUi();
        pullSelectedAccountIntoEditor();
    };
    addAndMakeVisible(addAccountButton);

    removeAccountButton.onClick = [this]
    {
        if (! juce::isPositiveAndBelow(selectedAccountIndex, aiSettings.accounts.size()))
            return;

        const auto removedAccountId = aiSettings.accounts.getReference(selectedAccountIndex).accountId;
        aiSettings.accounts.remove(selectedAccountIndex);

        if (aiSettings.defaultAccountId == removedAccountId)
            aiSettings.defaultAccountId = aiSettings.accounts.isEmpty() ? juce::String() : aiSettings.accounts[0].accountId;

        for (auto& selection : aiSettings.appSelections)
            if (selection.accountId == removedAccountId)
                selection.accountId.clear();

        selectedAccountIndex = juce::jlimit(0, aiSettings.accounts.size() - 1, selectedAccountIndex);
        if (aiSettings.accounts.isEmpty())
            selectedAccountIndex = -1;

        refreshAiAccountUi();
        pullSelectedAccountIntoEditor();
    };
    addAndMakeVisible(removeAccountButton);

    providerLabel.setText("Provider", juce::dontSendNotification);
    configureLabel(providerLabel);
    addAndMakeVisible(providerLabel);

    configureCombo(providerCombo);
    for (int index = 0; index < aiProviders.size(); ++index)
        providerCombo.addItem(aiProviders[index].displayName, comboItemIdForIndex(index));
    providerCombo.onChange = [this]
    {
        if (! juce::isPositiveAndBelow(selectedAccountIndex, aiSettings.accounts.size()))
            return;

        const auto providerIndex = providerCombo.getSelectedItemIndex();
        if (! juce::isPositiveAndBelow(providerIndex, aiProviders.size()))
            return;

        auto& account = aiSettings.accounts.getReference(selectedAccountIndex);
        const auto& provider = aiProviders.getReference(providerIndex);
        account.providerId = provider.id;
        if (account.baseUrl.trim().isEmpty())
            account.baseUrl = provider.defaultBaseUrl;
        if (account.providerId == "ollama" && account.modelName.trim().isEmpty())
            account.modelName = "llama3.1";
        pullSelectedAccountIntoEditor();
    };
    addAndMakeVisible(providerCombo);

    accountLabelLabel.setText("Account Label", juce::dontSendNotification);
    configureLabel(accountLabelLabel);
    addAndMakeVisible(accountLabelLabel);
    configureEditor(accountLabelEditor);
    addAndMakeVisible(accountLabelEditor);

    endpointLabel.setText("Endpoint", juce::dontSendNotification);
    configureLabel(endpointLabel);
    addAndMakeVisible(endpointLabel);
    configureEditor(endpointEditor);
    addAndMakeVisible(endpointEditor);

    modelLabel.setText("Default Model", juce::dontSendNotification);
    configureLabel(modelLabel);
    addAndMakeVisible(modelLabel);
    configureEditor(modelEditor);
    addAndMakeVisible(modelEditor);

    apiKeyLabel.setText("API Key / Token", juce::dontSendNotification);
    configureLabel(apiKeyLabel);
    addAndMakeVisible(apiKeyLabel);
    configureEditor(apiKeyEditor);
    apiKeyEditor.setPasswordCharacter(0x2022);
    addAndMakeVisible(apiKeyEditor);

    defaultAccountLabel.setText("Suite Default Account", juce::dontSendNotification);
    configureLabel(defaultAccountLabel);
    addAndMakeVisible(defaultAccountLabel);
    configureCombo(defaultAccountCombo);
    addAndMakeVisible(defaultAccountCombo);

    auto configureSelectionRow = [](AppSelectionRow& row, const juce::String& labelText)
    {
        row.label.setText(labelText, juce::dontSendNotification);
        configureLabel(row.label);
        configureCombo(row.accountCombo);
        configureEditor(row.modelOverrideEditor);
    };

    configureSelectionRow(stationSelectionRow, "Creation Station");
    configureSelectionRow(engineSelectionRow, "Creation Engine");
    configureSelectionRow(movieSelectionRow, "Creation Movie");
    configureSelectionRow(liveSelectionRow, "Creation Live");

    addAndMakeVisible(stationSelectionRow.label);
    addAndMakeVisible(stationSelectionRow.accountCombo);
    addAndMakeVisible(stationSelectionRow.modelOverrideEditor);
    addAndMakeVisible(engineSelectionRow.label);
    addAndMakeVisible(engineSelectionRow.accountCombo);
    addAndMakeVisible(engineSelectionRow.modelOverrideEditor);
    addAndMakeVisible(movieSelectionRow.label);
    addAndMakeVisible(movieSelectionRow.accountCombo);
    addAndMakeVisible(movieSelectionRow.modelOverrideEditor);
    addAndMakeVisible(liveSelectionRow.label);
    addAndMakeVisible(liveSelectionRow.accountCombo);
    addAndMakeVisible(liveSelectionRow.modelOverrideEditor);

    applyButton.onClick = [this]
    {
        pushCurrentEditorToSelectedAccount();
        if (onApplyRequested)
            onApplyRequested(getSettings());
        if (onApplyAiSettingsRequested)
            onApplyAiSettingsRequested(getAiSettings());
    };
    addAndMakeVisible(applyButton);

    eulaButton.onClick = [this]
    {
        if (onReadEulaRequested)
            onReadEulaRequested();
    };
    addAndMakeVisible(eulaButton);

    statusLabel.setColour(juce::Label::textColourId, hintColour());
    statusLabel.setText("Suite storage, AI accounts, and app routing are controlled here for every Creation app.",
                        juce::dontSendNotification);
    addAndMakeVisible(statusLabel);

    setAiSettings({});
}

void SuiteSettingsPanel::setSettings(const creation::suite::SuiteSettings& settings)
{
    suiteVfsRow.editor.setText(settings.suiteVfsRoot, juce::dontSendNotification);
    sharedResourcesRow.editor.setText(settings.sharedResourcesRoot, juce::dontSendNotification);
    projectContainersRow.editor.setText(settings.projectContainersRoot, juce::dontSendNotification);
    cacheRootRow.editor.setText(settings.cacheRoot, juce::dontSendNotification);
    materializedFilesRow.editor.setText(settings.materializedFilesRoot, juce::dontSendNotification);
    exportsRootRow.editor.setText(settings.exportsRoot, juce::dontSendNotification);
    stationProjectsRow.editor.setText(settings.creationStationProjectsRoot, juce::dontSendNotification);
    engineProjectsRow.editor.setText(settings.creationEngineProjectsRoot, juce::dontSendNotification);
    movieProjectsRow.editor.setText(settings.creationMovieProjectsRoot, juce::dontSendNotification);
    liveProjectsRow.editor.setText(settings.creationLiveProjectsRoot, juce::dontSendNotification);
}

creation::suite::SuiteSettings SuiteSettingsPanel::getSettings() const
{
    creation::suite::SuiteSettings settings;
    settings.suiteVfsRoot = suiteVfsRow.editor.getText().trim();
    settings.sharedResourcesRoot = sharedResourcesRow.editor.getText().trim();
    settings.projectContainersRoot = projectContainersRow.editor.getText().trim();
    settings.cacheRoot = cacheRootRow.editor.getText().trim();
    settings.materializedFilesRoot = materializedFilesRow.editor.getText().trim();
    settings.exportsRoot = exportsRootRow.editor.getText().trim();
    settings.creationStationProjectsRoot = stationProjectsRow.editor.getText().trim();
    settings.creationEngineProjectsRoot = engineProjectsRow.editor.getText().trim();
    settings.creationMovieProjectsRoot = movieProjectsRow.editor.getText().trim();
    settings.creationLiveProjectsRoot = liveProjectsRow.editor.getText().trim();
    return settings;
}

void SuiteSettingsPanel::setAiSettings(const creation::services::SuiteAiSettings& settings)
{
    aiSettings = settings;
    if (aiSettings.accounts.isEmpty())
    {
        creation::services::SuiteAiAccountSettings account;
        account.accountId = "suite-account-primary";
        account.providerId = "openai";
        account.accountLabel = "Primary Suite Account";
        account.baseUrl = "https://api.openai.com/v1";
        account.modelName = "gpt-4.1-mini";
        account.enabled = true;
        aiSettings.accounts.add(account);
        aiSettings.defaultAccountId = account.accountId;
    }

    selectedAccountIndex = juce::jlimit(0, aiSettings.accounts.size() - 1, 0);
    refreshAiAccountUi();
    pullSelectedAccountIntoEditor();
}

creation::services::SuiteAiSettings SuiteSettingsPanel::getAiSettings() const
{
    auto settings = aiSettings;

    auto updateSelection = [&](const juce::ComboBox& comboBox,
                               const juce::TextEditor& editor,
                               creation::assets::SuiteAppDomain appDomain)
    {
        auto* existing = const_cast<creation::services::SuiteAiSettings::AppSelection*>(
            creation::services::SuiteAiSettingsResolver::findAppSelection(settings, appDomain));
        if (existing != nullptr)
        {
            existing->accountId = accountIdForCombo(comboBox);
            existing->modelNameOverride = editor.getText().trim();
            existing->enabled = existing->accountId.isNotEmpty();
            return;
        }

        creation::services::SuiteAiSettings::AppSelection selection;
        selection.appDomain = appDomain;
        selection.accountId = accountIdForCombo(comboBox);
        selection.modelNameOverride = editor.getText().trim();
        selection.enabled = selection.accountId.isNotEmpty();
        settings.appSelections.add(selection);
    };

    settings.defaultAccountId = accountIdForCombo(defaultAccountCombo);
    updateSelection(stationSelectionRow.accountCombo, stationSelectionRow.modelOverrideEditor, creation::assets::SuiteAppDomain::station);
    updateSelection(engineSelectionRow.accountCombo, engineSelectionRow.modelOverrideEditor, creation::assets::SuiteAppDomain::engine);
    updateSelection(movieSelectionRow.accountCombo, movieSelectionRow.modelOverrideEditor, creation::assets::SuiteAppDomain::movie);
    updateSelection(liveSelectionRow.accountCombo, liveSelectionRow.modelOverrideEditor, creation::assets::SuiteAppDomain::live);

    return settings;
}

void SuiteSettingsPanel::setStatusText(const juce::String& text)
{
    statusLabel.setText(text, juce::dontSendNotification);
}

void SuiteSettingsPanel::paint(juce::Graphics& g)
{
    g.fillAll(panelFill());
    g.setColour(panelOutline());
    g.drawRect(getLocalBounds(), 1);
}

void SuiteSettingsPanel::resized()
{
    auto area = getLocalBounds().reduced(18);
    titleLabel.setBounds(area.removeFromTop(30));
    subTitleLabel.setBounds(area.removeFromTop(42));
    area.removeFromTop(10);

    storageSectionLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(8);

    layoutRow(suiteVfsRow, area);
    layoutRow(sharedResourcesRow, area);
    layoutRow(projectContainersRow, area);
    layoutRow(cacheRootRow, area);
    layoutRow(materializedFilesRow, area);
    layoutRow(exportsRootRow, area);
    layoutRow(stationProjectsRow, area);
    layoutRow(engineProjectsRow, area);
    layoutRow(movieProjectsRow, area);
    layoutRow(liveProjectsRow, area);

    area.removeFromTop(6);
    aiSectionLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(8);

    auto accountHeaderRow = area.removeFromTop(30);
    accountSelectorCombo.setBounds(accountHeaderRow.removeFromLeft(260));
    accountHeaderRow.removeFromLeft(8);
    addAccountButton.setBounds(accountHeaderRow.removeFromLeft(110));
    accountHeaderRow.removeFromLeft(8);
    removeAccountButton.setBounds(accountHeaderRow.removeFromLeft(90));
    area.removeFromTop(10);

    providerLabel.setBounds(area.removeFromTop(20));
    providerCombo.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);

    accountLabelLabel.setBounds(area.removeFromTop(20));
    accountLabelEditor.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);

    endpointLabel.setBounds(area.removeFromTop(20));
    endpointEditor.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);

    modelLabel.setBounds(area.removeFromTop(20));
    modelEditor.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);

    apiKeyLabel.setBounds(area.removeFromTop(20));
    apiKeyEditor.setBounds(area.removeFromTop(30));
    area.removeFromTop(14);

    defaultAccountLabel.setBounds(area.removeFromTop(20));
    defaultAccountCombo.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);

    auto layoutSelectionRow = [&](AppSelectionRow& row)
    {
        row.label.setBounds(area.removeFromTop(20));
        auto rowArea = area.removeFromTop(30);
        row.accountCombo.setBounds(rowArea.removeFromLeft(260));
        rowArea.removeFromLeft(8);
        row.modelOverrideEditor.setBounds(rowArea);
        area.removeFromTop(10);
    };

    layoutSelectionRow(stationSelectionRow);
    layoutSelectionRow(engineSelectionRow);
    layoutSelectionRow(movieSelectionRow);
    layoutSelectionRow(liveSelectionRow);

    area.removeFromTop(10);
    auto buttonRow = area.removeFromTop(34);
    applyButton.setBounds(buttonRow.removeFromLeft(180));
    buttonRow.removeFromLeft(10);
    eulaButton.setBounds(buttonRow.removeFromLeft(140));
    area.removeFromTop(8);
    statusLabel.setBounds(area.removeFromTop(44));
}

void SuiteSettingsPanel::configureRow(PathRow& row, const juce::String& id, const juce::String& labelText)
{
    row.id = id;
    row.label.setText(labelText, juce::dontSendNotification);
    configureLabel(row.label);
    addAndMakeVisible(row.label);

    configureEditor(row.editor);
    addAndMakeVisible(row.editor);

    row.browseButton.onClick = [this, id]
    {
        if (onBrowseRequested)
            onBrowseRequested(id);
    };
    addAndMakeVisible(row.browseButton);
}

void SuiteSettingsPanel::layoutRow(PathRow& row, juce::Rectangle<int>& area)
{
    row.label.setBounds(area.removeFromTop(20));
    auto rowArea = area.removeFromTop(30);
    row.browseButton.setBounds(rowArea.removeFromRight(88));
    rowArea.removeFromRight(8);
    row.editor.setBounds(rowArea);
    area.removeFromTop(10);
}

void SuiteSettingsPanel::refreshAiAccountUi()
{
    accountSelectorCombo.clear(juce::dontSendNotification);
    for (int index = 0; index < aiSettings.accounts.size(); ++index)
    {
        const auto& account = aiSettings.accounts.getReference(index);
        auto label = account.accountLabel.trim();
        if (label.isEmpty())
            label = "Suite Account " + juce::String(index + 1);
        accountSelectorCombo.addItem(label, comboItemIdForIndex(index));
    }

    if (aiSettings.accounts.isEmpty())
        selectedAccountIndex = -1;
    else if (! juce::isPositiveAndBelow(selectedAccountIndex, aiSettings.accounts.size()))
        selectedAccountIndex = 0;

    accountSelectorCombo.setSelectedItemIndex(selectedAccountIndex, juce::dontSendNotification);
    removeAccountButton.setEnabled(aiSettings.accounts.size() > 1);
    refreshAccountSelectors();
}

void SuiteSettingsPanel::refreshAccountSelectors()
{
    auto fillAccountCombo = [&](juce::ComboBox& comboBox, const juce::String& selectedAccountId)
    {
        comboBox.clear(juce::dontSendNotification);
        comboBox.addItem("(Use Suite Default)", 1);
        for (int index = 0; index < aiSettings.accounts.size(); ++index)
        {
            const auto& account = aiSettings.accounts.getReference(index);
            auto label = account.accountLabel.trim();
            if (label.isEmpty())
                label = "Suite Account " + juce::String(index + 1);
            comboBox.addItem(label, index + 2);
            if (account.accountId == selectedAccountId)
                comboBox.setSelectedId(index + 2, juce::dontSendNotification);
        }

        if (selectedAccountId.isEmpty())
            comboBox.setSelectedId(1, juce::dontSendNotification);
    };

    defaultAccountCombo.clear(juce::dontSendNotification);
    for (int index = 0; index < aiSettings.accounts.size(); ++index)
    {
        const auto& account = aiSettings.accounts.getReference(index);
        auto label = account.accountLabel.trim();
        if (label.isEmpty())
            label = "Suite Account " + juce::String(index + 1);
        defaultAccountCombo.addItem(label, comboItemIdForIndex(index));
        if (account.accountId == aiSettings.defaultAccountId)
            defaultAccountCombo.setSelectedId(comboItemIdForIndex(index), juce::dontSendNotification);
    }
    if (defaultAccountCombo.getSelectedId() == 0 && aiSettings.accounts.size() > 0)
        defaultAccountCombo.setSelectedId(1, juce::dontSendNotification);

    auto applySelection = [&](AppSelectionRow& row, creation::assets::SuiteAppDomain domain)
    {
        const auto* selection = creation::services::SuiteAiSettingsResolver::findAppSelection(aiSettings, domain);
        fillAccountCombo(row.accountCombo, selection != nullptr ? selection->accountId : juce::String());
        row.modelOverrideEditor.setText(selection != nullptr ? selection->modelNameOverride : juce::String(),
                                        juce::dontSendNotification);
    };

    applySelection(stationSelectionRow, creation::assets::SuiteAppDomain::station);
    applySelection(engineSelectionRow, creation::assets::SuiteAppDomain::engine);
    applySelection(movieSelectionRow, creation::assets::SuiteAppDomain::movie);
    applySelection(liveSelectionRow, creation::assets::SuiteAppDomain::live);
}

void SuiteSettingsPanel::pushCurrentEditorToSelectedAccount()
{
    if (! juce::isPositiveAndBelow(selectedAccountIndex, aiSettings.accounts.size()))
        return;

    auto& account = aiSettings.accounts.getReference(selectedAccountIndex);
    account.accountLabel = accountLabelEditor.getText().trim();
    account.baseUrl = endpointEditor.getText().trim();
    account.modelName = modelEditor.getText().trim();
    account.apiKey = apiKeyEditor.getText();

    const auto providerIndex = providerCombo.getSelectedItemIndex();
    if (juce::isPositiveAndBelow(providerIndex, aiProviders.size()))
        account.providerId = aiProviders.getReference(providerIndex).id;
}

void SuiteSettingsPanel::pullSelectedAccountIntoEditor()
{
    if (! juce::isPositiveAndBelow(selectedAccountIndex, aiSettings.accounts.size()))
    {
        providerCombo.setSelectedId(0, juce::dontSendNotification);
        accountLabelEditor.clear();
        endpointEditor.clear();
        modelEditor.clear();
        apiKeyEditor.clear();
        return;
    }

    const auto& account = aiSettings.accounts.getReference(selectedAccountIndex);
    accountLabelEditor.setText(account.accountLabel, juce::dontSendNotification);
    endpointEditor.setText(account.baseUrl, juce::dontSendNotification);
    modelEditor.setText(account.modelName, juce::dontSendNotification);
    apiKeyEditor.setText(account.apiKey, juce::dontSendNotification);

    auto providerIndex = 0;
    for (int index = 0; index < aiProviders.size(); ++index)
        if (aiProviders[index].id == account.providerId)
            providerIndex = index;
    providerCombo.setSelectedId(comboItemIdForIndex(providerIndex), juce::dontSendNotification);
}

juce::String SuiteSettingsPanel::accountIdForCombo(const juce::ComboBox& comboBox) const
{
    const auto selectedId = comboBox.getSelectedId();
    if (selectedId <= 1)
        return {};

    const auto accountIndex = selectedId - 2;
    if (! juce::isPositiveAndBelow(accountIndex, aiSettings.accounts.size()))
        return {};

    return aiSettings.accounts[accountIndex].accountId;
}
