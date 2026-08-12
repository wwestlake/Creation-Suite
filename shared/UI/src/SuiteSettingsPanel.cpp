#include <creation/ui/SuiteSettingsPanel.h>
#include <creation/ui/CreationSuiteLogos.h>
#include <creation/services/SuiteAiProviderRuntime.h>
#if JUCE_DEBUG
#include <creation/services/SuiteVfsServiceClient.h>
#include <creation/suite/SuiteStoragePaths.h>
#endif

namespace
{
juce::Colour panelFill() { return juce::Colour(0xff0f141c); }
juce::Colour panelOutline() { return juce::Colour(0xff2a3a50); }
juce::Colour cardFill() { return juce::Colour(0xff121822); }
juce::Colour contentFill() { return juce::Colour(0xff141c27); }
juce::Colour textColour() { return juce::Colour(0xffdce6f5); }
juce::Colour hintColour() { return juce::Colour(0xff97a9c1); }
juce::Colour accentColour() { return juce::Colour(0xff59dfff); }

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
    label.setColour(juce::Label::textColourId, textColour());
}

void configureSectionTitle(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(20.0f).boldened());
    label.setColour(juce::Label::textColourId, juce::Colours::white);
}

void configureHintLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, hintColour());
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
    suiteLogo = creation::ui::getSuiteLogoImage(creation::ui::SuiteLogoId::suite);

    titleLabel.setText("Creation Suite", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(28.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    subTitleLabel.setText("Suite-wide storage, routing, accounts, and platform controls live here.",
                          juce::dontSendNotification);
    subTitleLabel.setColour(juce::Label::textColourId, hintColour());
    addAndMakeVisible(subTitleLabel);

    configureTabButton(storageTabButton, "Storage", Tab::storage);
    configureTabButton(aiTabButton, "AI & Routing", Tab::ai);
    configureTabButton(suiteLogTabButton, "Suite Log", Tab::log);
#if JUCE_DEBUG
    configureTabButton(vfsBrowserTabButton, "VFS Browser", Tab::vfsBrowser);

    vfsTreeView.setColour(juce::TreeView::backgroundColourId, juce::Colour(0xff1a2230));
    vfsTreeView.setColour(juce::TreeView::linesColourId, panelOutline());
    addChildComponent(vfsTreeView);

    vfsContentTitleLabel.setFont(juce::Font(13.0f).boldened());
    configureLabel(vfsContentTitleLabel);
    addChildComponent(vfsContentTitleLabel);

    vfsContentViewer.setMultiLine(true);
    vfsContentViewer.setReadOnly(true);
    vfsContentViewer.setScrollbarsShown(true);
    vfsContentViewer.setCaretVisible(false);
    vfsContentViewer.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
    configureEditor(vfsContentViewer);
    vfsContentViewer.setText("Select a file or entry on the left.", juce::dontSendNotification);
    addChildComponent(vfsContentViewer);
#endif

    scrollViewport.setViewedComponent(&scrollContent, false);
    scrollViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(scrollViewport);

    configureHintLabel(storageIntroLabel,
                       "Set one suite VFS root here. Everything else in the Creation Suite is derived inside that VFS automatically.");
    scrollContent.addAndMakeVisible(storageIntroLabel);

    configureSectionTitle(storageSectionLabel, "Storage And VFS");
    scrollContent.addAndMakeVisible(storageSectionLabel);

    configureRow(suiteVfsRow, "suite_vfs_root", "Suite VFS Root");

    configureHintLabel(aiIntroLabel,
                       "Shared accounts and per-app routing live here so every app in the suite can resolve AI behavior from one place.");
    scrollContent.addAndMakeVisible(aiIntroLabel);

    configureSectionTitle(aiSectionLabel, "Suite AI Accounts And Routing");
    scrollContent.addAndMakeVisible(aiSectionLabel);

    configureCombo(accountSelectorCombo);
    accountSelectorCombo.onChange = [this]
    {
        pushCurrentEditorToSelectedAccount();
        selectedAccountIndex = accountSelectorCombo.getSelectedItemIndex();
        pullSelectedAccountIntoEditor();
        refreshAiAccountUi();
    };
    scrollContent.addAndMakeVisible(accountSelectorCombo);

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
        appendLogLine("Added a new suite AI account slot.");
    };
    scrollContent.addAndMakeVisible(addAccountButton);

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
        appendLogLine("Removed a suite AI account slot.");
    };
    scrollContent.addAndMakeVisible(removeAccountButton);

    testAccountButton.onClick = [this]
    {
        pushCurrentEditorToSelectedAccount();
        if (! juce::isPositiveAndBelow(selectedAccountIndex, aiSettings.accounts.size()))
        {
            setStatusText("Select an AI account first.");
            return;
        }

        const auto& account = aiSettings.accounts.getReference(selectedAccountIndex);
        creation::services::SuiteAiResolvedRuntimeSettings runtimeSettings;
        runtimeSettings.accountId = account.accountId;
        runtimeSettings.providerId = account.providerId;
        runtimeSettings.providerDisplayName = creation::services::SuiteAiProviderRuntime::resolveProfile(account.providerId).displayName;
        runtimeSettings.baseUrl = account.baseUrl;
        runtimeSettings.modelName = account.modelName;
        runtimeSettings.apiKey = account.apiKey;

        setStatusText("Testing " + runtimeSettings.providerDisplayName + " account...");
        if (onTestAiAccountRequested)
            onTestAiAccountRequested(runtimeSettings);
    };
    scrollContent.addAndMakeVisible(testAccountButton);

    providerLabel.setText("Provider", juce::dontSendNotification);
    configureLabel(providerLabel);
    scrollContent.addAndMakeVisible(providerLabel);

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
        if ((account.providerId == "ollama" || account.providerId == "lm-studio") && account.modelName.trim().isEmpty())
            account.modelName = account.providerId == "ollama" ? "llama3.1" : "local-model";
        pullSelectedAccountIntoEditor();
    };
    scrollContent.addAndMakeVisible(providerCombo);

    accountLabelLabel.setText("Account Label", juce::dontSendNotification);
    configureLabel(accountLabelLabel);
    scrollContent.addAndMakeVisible(accountLabelLabel);
    configureEditor(accountLabelEditor);
    scrollContent.addAndMakeVisible(accountLabelEditor);

    endpointLabel.setText("Endpoint", juce::dontSendNotification);
    configureLabel(endpointLabel);
    scrollContent.addAndMakeVisible(endpointLabel);
    configureEditor(endpointEditor);
    scrollContent.addAndMakeVisible(endpointEditor);

    modelLabel.setText("Default Model", juce::dontSendNotification);
    configureLabel(modelLabel);
    scrollContent.addAndMakeVisible(modelLabel);
    configureEditor(modelEditor);
    scrollContent.addAndMakeVisible(modelEditor);

    apiKeyLabel.setText("API Key / Token", juce::dontSendNotification);
    configureLabel(apiKeyLabel);
    scrollContent.addAndMakeVisible(apiKeyLabel);
    configureEditor(apiKeyEditor);
    apiKeyEditor.setPasswordCharacter(0x2022);
    scrollContent.addAndMakeVisible(apiKeyEditor);

    defaultAccountLabel.setText("Suite Default Account", juce::dontSendNotification);
    configureLabel(defaultAccountLabel);
    scrollContent.addAndMakeVisible(defaultAccountLabel);
    configureCombo(defaultAccountCombo);
    scrollContent.addAndMakeVisible(defaultAccountCombo);

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

    scrollContent.addAndMakeVisible(stationSelectionRow.label);
    scrollContent.addAndMakeVisible(stationSelectionRow.accountCombo);
    scrollContent.addAndMakeVisible(stationSelectionRow.modelOverrideEditor);
    scrollContent.addAndMakeVisible(engineSelectionRow.label);
    scrollContent.addAndMakeVisible(engineSelectionRow.accountCombo);
    scrollContent.addAndMakeVisible(engineSelectionRow.modelOverrideEditor);
    scrollContent.addAndMakeVisible(movieSelectionRow.label);
    scrollContent.addAndMakeVisible(movieSelectionRow.accountCombo);
    scrollContent.addAndMakeVisible(movieSelectionRow.modelOverrideEditor);
    scrollContent.addAndMakeVisible(liveSelectionRow.label);
    scrollContent.addAndMakeVisible(liveSelectionRow.accountCombo);
    scrollContent.addAndMakeVisible(liveSelectionRow.modelOverrideEditor);

    configureSectionTitle(logSectionLabel, "Suite Log");
    scrollContent.addAndMakeVisible(logSectionLabel);

    configureHintLabel(logIntroLabel,
                       "This is the shared suite control log. As the suite grows, this is where setup status, shared warnings, migrations, and platform-level actions can accumulate.");
    scrollContent.addAndMakeVisible(logIntroLabel);

    suiteLogEditor.setMultiLine(true);
    suiteLogEditor.setReadOnly(true);
    suiteLogEditor.setScrollbarsShown(true);
    suiteLogEditor.setCaretVisible(false);
    suiteLogEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1a2230));
    suiteLogEditor.setColour(juce::TextEditor::outlineColourId, panelOutline());
    suiteLogEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    scrollContent.addAndMakeVisible(suiteLogEditor);

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
    addAndMakeVisible(statusLabel);

    setAiSettings({});
    selectTab(Tab::storage);
    setStatusText("Suite storage, AI accounts, and app routing are controlled here for every Creation app.");
}

void SuiteSettingsPanel::setSettings(const creation::suite::SuiteSettings& settings)
{
    suiteVfsRow.editor.setText(settings.suiteVfsRoot, juce::dontSendNotification);
#if JUCE_DEBUG
    currentSettings = settings;
    buildVfsBrowserTree();
#endif
}

creation::suite::SuiteSettings SuiteSettingsPanel::getSettings() const
{
    creation::suite::SuiteSettings settings;
    settings.suiteVfsRoot = suiteVfsRow.editor.getText().trim();
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
    appendLogLine(text);
}

void SuiteSettingsPanel::paint(juce::Graphics& g)
{
    g.fillAll(panelFill());
    g.setColour(panelOutline());
    g.drawRect(getLocalBounds(), 1);

    auto area = getLocalBounds().reduced(14);
    auto headerArea = area.removeFromTop(94).toFloat();
    auto tabArea = area.removeFromTop(38).toFloat();
    auto footerArea = area.removeFromBottom(78).toFloat();

    g.setColour(cardFill());
    g.fillRoundedRectangle(headerArea, 18.0f);
    g.setColour(panelOutline());
    g.drawRoundedRectangle(headerArea, 18.0f, 1.0f);

    g.setColour(juce::Colour(0xff151b23));
    g.fillRoundedRectangle(tabArea, 14.0f);
    g.setColour(panelOutline());
    g.drawRoundedRectangle(tabArea, 14.0f, 1.0f);

    g.setColour(cardFill());
    g.fillRoundedRectangle(footerArea, 18.0f);
    g.setColour(panelOutline());
    g.drawRoundedRectangle(footerArea, 18.0f, 1.0f);

    if (suiteLogo.isValid())
        g.drawImageWithin(suiteLogo, 28, 24, 56, 56, juce::RectanglePlacement::centred, false);
}

void SuiteSettingsPanel::resized()
{
    auto area = getLocalBounds().reduced(18);

    auto headerArea = area.removeFromTop(86);
    titleLabel.setBounds(headerArea.removeFromLeft(420).withTrimmedLeft(72).withTrimmedTop(6));
    subTitleLabel.setBounds(headerArea.withTrimmedLeft(72).withTrimmedTop(36));

    area.removeFromTop(8);
    auto tabArea = area.removeFromTop(34);
    auto tabButtonWidth = 136;
    storageTabButton.setBounds(tabArea.removeFromLeft(tabButtonWidth));
    tabArea.removeFromLeft(8);
    aiTabButton.setBounds(tabArea.removeFromLeft(tabButtonWidth + 20));
    tabArea.removeFromLeft(8);
    suiteLogTabButton.setBounds(tabArea.removeFromLeft(tabButtonWidth));
#if JUCE_DEBUG
    tabArea.removeFromLeft(8);
    vfsBrowserTabButton.setBounds(tabArea.removeFromLeft(tabButtonWidth + 20));
#endif

    area.removeFromTop(10);
    auto footerArea = area.removeFromBottom(74);
    scrollViewport.setBounds(area);
#if JUCE_DEBUG
    auto vfsArea = area;
    vfsTreeView.setBounds(vfsArea.removeFromLeft(280));
    vfsArea.removeFromLeft(10);
    vfsContentTitleLabel.setBounds(vfsArea.removeFromTop(22));
    vfsArea.removeFromTop(4);
    vfsContentViewer.setBounds(vfsArea);
#endif

    auto footerButtons = footerArea.removeFromLeft(338);
    applyButton.setBounds(footerButtons.removeFromLeft(180));
    footerButtons.removeFromLeft(10);
    eulaButton.setBounds(footerButtons.removeFromLeft(140));
    statusLabel.setBounds(footerArea.withTrimmedLeft(16));

    updateScrollContentLayout();
}

void SuiteSettingsPanel::configureRow(PathRow& row, const juce::String& id, const juce::String& labelText)
{
    row.id = id;
    row.label.setText(labelText, juce::dontSendNotification);
    configureLabel(row.label);
    scrollContent.addAndMakeVisible(row.label);

    configureEditor(row.editor);
    scrollContent.addAndMakeVisible(row.editor);

    row.browseButton.onClick = [this, id]
    {
        if (onBrowseRequested)
            onBrowseRequested(id);
    };
    scrollContent.addAndMakeVisible(row.browseButton);
}

void SuiteSettingsPanel::layoutRow(PathRow& row, juce::Rectangle<int>& area)
{
    row.label.setBounds(area.removeFromTop(20));
    auto rowArea = area.removeFromTop(32);
    row.browseButton.setBounds(rowArea.removeFromRight(96));
    rowArea.removeFromRight(8);
    row.editor.setBounds(rowArea);
    area.removeFromTop(12);
}

void SuiteSettingsPanel::configureTabButton(juce::TextButton& button, const juce::String& text, Tab tab)
{
    button.setButtonText(text);
    button.onClick = [this, tab] { selectTab(tab); };
    addAndMakeVisible(button);
}

void SuiteSettingsPanel::selectTab(Tab tab)
{
    selectedTab = tab;
    refreshTabButtons();

#if JUCE_DEBUG
    const bool isVfsBrowser = (selectedTab == Tab::vfsBrowser);
    scrollViewport.setVisible(! isVfsBrowser);
    vfsTreeView.setVisible(isVfsBrowser);
    vfsContentTitleLabel.setVisible(isVfsBrowser);
    vfsContentViewer.setVisible(isVfsBrowser);
#endif

    updateScrollContentLayout();
    scrollViewport.setViewPosition(0, 0);
}

void SuiteSettingsPanel::refreshTabButtons()
{
    auto styleTab = [&](juce::TextButton& button, bool active)
    {
        button.setColour(juce::TextButton::buttonColourId, active ? accentColour().withAlpha(0.18f) : juce::Colour(0xff151b23));
        button.setColour(juce::TextButton::buttonOnColourId, accentColour().withAlpha(0.18f));
        button.setColour(juce::TextButton::textColourOffId, active ? juce::Colours::white : juce::Colour(0xffb8c4d5));
        button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    };

    styleTab(storageTabButton, selectedTab == Tab::storage);
    styleTab(aiTabButton, selectedTab == Tab::ai);
    styleTab(suiteLogTabButton, selectedTab == Tab::log);
#if JUCE_DEBUG
    styleTab(vfsBrowserTabButton, selectedTab == Tab::vfsBrowser);
#endif
}

void SuiteSettingsPanel::updateScrollContentLayout()
{
    const auto contentWidth = juce::jmax(640, scrollViewport.getWidth() - 12);
    juce::Rectangle<int> area(22, 20, juce::jmax(400, contentWidth - 44), 20000);

    auto setPathRowsVisible = [&](bool visible)
    {
        auto setRowVisible = [visible](PathRow& row)
        {
            row.label.setVisible(visible);
            row.editor.setVisible(visible);
            row.browseButton.setVisible(visible);
        };

        storageIntroLabel.setVisible(visible);
        storageSectionLabel.setVisible(visible);
        setRowVisible(suiteVfsRow);
    };

    auto setAiVisible = [&](bool visible)
    {
        aiIntroLabel.setVisible(visible);
        aiSectionLabel.setVisible(visible);
        accountSelectorCombo.setVisible(visible);
        addAccountButton.setVisible(visible);
        removeAccountButton.setVisible(visible);
        testAccountButton.setVisible(visible);
        providerLabel.setVisible(visible);
        providerCombo.setVisible(visible);
        accountLabelLabel.setVisible(visible);
        accountLabelEditor.setVisible(visible);
        endpointLabel.setVisible(visible);
        endpointEditor.setVisible(visible);
        modelLabel.setVisible(visible);
        modelEditor.setVisible(visible);
        apiKeyLabel.setVisible(visible);
        apiKeyEditor.setVisible(visible);
        defaultAccountLabel.setVisible(visible);
        defaultAccountCombo.setVisible(visible);
        stationSelectionRow.label.setVisible(visible);
        stationSelectionRow.accountCombo.setVisible(visible);
        stationSelectionRow.modelOverrideEditor.setVisible(visible);
        engineSelectionRow.label.setVisible(visible);
        engineSelectionRow.accountCombo.setVisible(visible);
        engineSelectionRow.modelOverrideEditor.setVisible(visible);
        movieSelectionRow.label.setVisible(visible);
        movieSelectionRow.accountCombo.setVisible(visible);
        movieSelectionRow.modelOverrideEditor.setVisible(visible);
        liveSelectionRow.label.setVisible(visible);
        liveSelectionRow.accountCombo.setVisible(visible);
        liveSelectionRow.modelOverrideEditor.setVisible(visible);
    };

    auto setLogVisible = [&](bool visible)
    {
        logSectionLabel.setVisible(visible);
        logIntroLabel.setVisible(visible);
        suiteLogEditor.setVisible(visible);
    };

    setPathRowsVisible(selectedTab == Tab::storage);
    setAiVisible(selectedTab == Tab::ai);
    setLogVisible(selectedTab == Tab::log);

    switch (selectedTab)
    {
        case Tab::storage: layoutStorageTab(area); break;
        case Tab::ai: layoutAiTab(area); break;
        case Tab::log: layoutLogTab(area); break;
#if JUCE_DEBUG
        case Tab::vfsBrowser: break; // laid out directly in resized(), not the scrollContent area.
#endif
    }

    const auto usedHeight = 20000 - area.getHeight() + 18;
    scrollContent.setSize(contentWidth, usedHeight);
}

void SuiteSettingsPanel::layoutStorageTab(juce::Rectangle<int>& area)
{
    storageIntroLabel.setBounds(area.removeFromTop(48));
    area.removeFromTop(10);
    storageSectionLabel.setBounds(area.removeFromTop(28));
    area.removeFromTop(12);

    layoutRow(suiteVfsRow, area);
}

void SuiteSettingsPanel::layoutAiTab(juce::Rectangle<int>& area)
{
    aiIntroLabel.setBounds(area.removeFromTop(48));
    area.removeFromTop(10);
    aiSectionLabel.setBounds(area.removeFromTop(28));
    area.removeFromTop(12);

    auto accountHeaderRow = area.removeFromTop(34);
    accountSelectorCombo.setBounds(accountHeaderRow.removeFromLeft(280));
    accountHeaderRow.removeFromLeft(8);
    addAccountButton.setBounds(accountHeaderRow.removeFromLeft(118));
    accountHeaderRow.removeFromLeft(8);
    removeAccountButton.setBounds(accountHeaderRow.removeFromLeft(96));
    accountHeaderRow.removeFromLeft(8);
    testAccountButton.setBounds(accountHeaderRow.removeFromLeft(128));
    area.removeFromTop(12);

    providerLabel.setBounds(area.removeFromTop(20));
    providerCombo.setBounds(area.removeFromTop(32));
    area.removeFromTop(10);

    accountLabelLabel.setBounds(area.removeFromTop(20));
    accountLabelEditor.setBounds(area.removeFromTop(32));
    area.removeFromTop(10);

    endpointLabel.setBounds(area.removeFromTop(20));
    endpointEditor.setBounds(area.removeFromTop(32));
    area.removeFromTop(10);

    modelLabel.setBounds(area.removeFromTop(20));
    modelEditor.setBounds(area.removeFromTop(32));
    area.removeFromTop(10);

    apiKeyLabel.setBounds(area.removeFromTop(20));
    apiKeyEditor.setBounds(area.removeFromTop(32));
    area.removeFromTop(14);

    defaultAccountLabel.setBounds(area.removeFromTop(20));
    defaultAccountCombo.setBounds(area.removeFromTop(32));
    area.removeFromTop(10);

    auto layoutSelectionRow = [&](AppSelectionRow& row)
    {
        row.label.setBounds(area.removeFromTop(20));
        auto rowArea = area.removeFromTop(32);
        row.accountCombo.setBounds(rowArea.removeFromLeft(280));
        rowArea.removeFromLeft(8);
        row.modelOverrideEditor.setBounds(rowArea);
        area.removeFromTop(12);
    };

    layoutSelectionRow(stationSelectionRow);
    layoutSelectionRow(engineSelectionRow);
    layoutSelectionRow(movieSelectionRow);
    layoutSelectionRow(liveSelectionRow);
}

void SuiteSettingsPanel::layoutLogTab(juce::Rectangle<int>& area)
{
    logSectionLabel.setBounds(area.removeFromTop(28));
    area.removeFromTop(8);
    logIntroLabel.setBounds(area.removeFromTop(48));
    area.removeFromTop(12);
    suiteLogEditor.setBounds(area.removeFromTop(520));
}

void SuiteSettingsPanel::appendLogLine(const juce::String& text)
{
    const auto trimmed = text.trim();
    if (trimmed.isEmpty())
        return;

    const auto timestamp = juce::Time::getCurrentTime().formatted("%H:%M:%S");
    const auto entry = "[" + timestamp + "] " + trimmed;
    const auto existing = suiteLogEditor.getText().trimEnd();
    if (existing.endsWith(entry))
        return;

    suiteLogEditor.moveCaretToEnd();
    if (existing.isNotEmpty())
        suiteLogEditor.insertTextAtCaret("\n");
    suiteLogEditor.insertTextAtCaret(entry);
    suiteLogEditor.moveCaretToEnd();
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

#if JUCE_DEBUG

SuiteSettingsPanel::VfsFileTreeItem::VfsFileTreeItem(const juce::File& file, std::function<void(const juce::File&)> onSelected)
    : file_(file), onSelected_(std::move(onSelected))
{
}

bool SuiteSettingsPanel::VfsFileTreeItem::mightContainSubItems()
{
    return file_.isDirectory();
}

void SuiteSettingsPanel::VfsFileTreeItem::itemOpennessChanged(bool isNowOpen)
{
    if (! isNowOpen || getNumSubItems() > 0)
        return;

    juce::Array<juce::File> dirs, files;
    juce::Array<juce::File> children;
    file_.findChildFiles(children, juce::File::findFilesAndDirectories, false, "*");
    for (const auto& child : children)
        (child.isDirectory() ? dirs : files).add(child);

    dirs.sort();
    files.sort();
    for (const auto& d : dirs)
        addSubItem(new VfsFileTreeItem(d, onSelected_));
    for (const auto& f : files)
        addSubItem(new VfsFileTreeItem(f, onSelected_));
}

void SuiteSettingsPanel::VfsFileTreeItem::paintItem(juce::Graphics& g, int width, int height)
{
    g.setColour(juce::Colours::white);
    g.setFont(13.0f);
    auto name = file_.getFileName();
    if (name.isEmpty())
        name = file_.getFullPathName();
    if (! file_.isDirectory())
        name << "   (" << juce::String(file_.getSize()) << " bytes)";
    g.drawText(name, 2, 0, width - 4, height, juce::Justification::centredLeft, true);
}

void SuiteSettingsPanel::VfsFileTreeItem::itemClicked(const juce::MouseEvent&)
{
    if (file_.isDirectory())
        setOpen(! isOpen());
    else if (onSelected_)
        onSelected_(file_);
}

SuiteSettingsPanel::VfsEntryTreeItem::VfsEntryTreeItem(juce::String displayName, juce::String fullLogicalPath, bool isLeaf,
                                                       std::function<void(const juce::String&)> onSelected)
    : displayName_(std::move(displayName)), fullLogicalPath_(std::move(fullLogicalPath)), isLeaf_(isLeaf),
      onSelected_(std::move(onSelected))
{
}

void SuiteSettingsPanel::VfsEntryTreeItem::addChildPath(const juce::StringArray& remainingSegments, const juce::String& fullLogicalPath)
{
    if (remainingSegments.isEmpty())
        return;

    if (remainingSegments.size() == 1)
    {
        addSubItem(new VfsEntryTreeItem(remainingSegments[0], fullLogicalPath, true, onSelected_));
        return;
    }

    auto* child = findOrCreateChild(remainingSegments[0]);
    juce::StringArray rest;
    for (int i = 1; i < remainingSegments.size(); ++i)
        rest.add(remainingSegments[i]);
    child->addChildPath(rest, fullLogicalPath);
}

SuiteSettingsPanel::VfsEntryTreeItem* SuiteSettingsPanel::VfsEntryTreeItem::findOrCreateChild(const juce::String& segment)
{
    for (int i = 0; i < getNumSubItems(); ++i)
    {
        auto* existing = dynamic_cast<VfsEntryTreeItem*>(getSubItem(i));
        if (existing != nullptr && existing->displayName_ == segment)
            return existing;
    }

    auto* created = new VfsEntryTreeItem(segment, juce::String(), false, onSelected_);
    addSubItem(created);
    return created;
}

void SuiteSettingsPanel::VfsEntryTreeItem::paintItem(juce::Graphics& g, int width, int height)
{
    g.setColour(juce::Colours::white);
    g.setFont(13.0f);
    g.drawText(displayName_, 2, 0, width - 4, height, juce::Justification::centredLeft, true);
}

void SuiteSettingsPanel::VfsEntryTreeItem::itemClicked(const juce::MouseEvent&)
{
    if (isLeaf_)
    {
        if (onSelected_)
            onSelected_(fullLogicalPath_);
    }
    else
    {
        setOpen(! isOpen());
    }
}

void SuiteSettingsPanel::buildVfsBrowserTree()
{
    vfsTreeView.setRootItem(nullptr);
    vfsTreeRoot.reset();

    auto invisibleRoot = std::make_unique<VfsEntryTreeItem>("root", juce::String(), false, nullptr);

    auto fsRoot = creation::suite::getSuiteRootDirectory(currentSettings);
    invisibleRoot->addSubItem(new VfsFileTreeItem(fsRoot, [this](const juce::File& file) { showVfsFileContent(file); }));

    auto* entriesRoot = new VfsEntryTreeItem("Suite Entries (via VFS service)", juce::String(), false,
        [this](const juce::String& path) { showVfsEntryContent(path); });

    creation::services::SuiteVfsServiceClient client;
    juce::StringArray entryPaths;
    if (client.discover() && client.listEntries(entryPaths))
    {
        for (const auto& path : entryPaths)
        {
            auto segments = juce::StringArray::fromTokens(path, "/", "");
            segments.removeEmptyStrings();
            if (! segments.isEmpty())
                entriesRoot->addChildPath(segments, path);
        }
    }
    invisibleRoot->addSubItem(entriesRoot);

    vfsTreeRoot = std::move(invisibleRoot);
    vfsTreeView.setRootItem(vfsTreeRoot.get());
    vfsTreeView.setRootItemVisible(false);
    vfsTreeRoot->setOpen(true);
}

void SuiteSettingsPanel::showVfsFileContent(const juce::File& file)
{
    if (file.hasFileExtension(".csproj"))
    {
        setVfsContentText(file.getFullPathName(), {}, false,
            "Binary project container (" + juce::String(file.getSize()) + " bytes). Browsing "
            "container internals isn't supported yet -- see docs/Suite-VFS-Browser-Debug-Tool.md.");
        return;
    }

    if (! file.existsAsFile())
    {
        setVfsContentText(file.getFullPathName(), {}, false, "(a folder -- select a file to view its content)");
        return;
    }

    setVfsContentText(file.getFullPathName(), file.loadFileAsString(), true, {});
}

void SuiteSettingsPanel::showVfsEntryContent(const juce::String& logicalPath)
{
    creation::services::SuiteVfsServiceClient client;
    if (! client.discover())
    {
        setVfsContentText(logicalPath, {}, false, "Could not reach the suite VFS service.");
        return;
    }

    juce::MemoryBlock data;
    if (! client.readEntry(logicalPath, data))
    {
        setVfsContentText(logicalPath, {}, false, "Could not read this entry.");
        return;
    }

    setVfsContentText(logicalPath, juce::String::createStringFromData(data.getData(), static_cast<int>(data.getSize())), true, {});
}

void SuiteSettingsPanel::setVfsContentText(const juce::String& title, const juce::String& rawContent, bool wasParsed, const juce::String& parseNote)
{
    vfsContentTitleLabel.setText(title, juce::dontSendNotification);

    if (! wasParsed)
    {
        vfsContentViewer.setText(parseNote, juce::dontSendNotification);
        return;
    }

    if (title.endsWithIgnoreCase(".json"))
    {
        const auto parsed = juce::JSON::parse(rawContent);
        if (! parsed.isVoid())
        {
            vfsContentViewer.setText(juce::JSON::toString(parsed, true), juce::dontSendNotification);
            return;
        }
        vfsContentViewer.setText("(not valid JSON -- showing raw content)\n\n" + rawContent, juce::dontSendNotification);
        return;
    }

    if (title.endsWithIgnoreCase(".xml"))
    {
        if (auto xml = juce::parseXML(rawContent))
        {
            vfsContentViewer.setText(xml->toString(), juce::dontSendNotification);
            return;
        }
        vfsContentViewer.setText("(not valid XML -- showing raw content)\n\n" + rawContent, juce::dontSendNotification);
        return;
    }

    vfsContentViewer.setText(rawContent, juce::dontSendNotification);
}

#endif
