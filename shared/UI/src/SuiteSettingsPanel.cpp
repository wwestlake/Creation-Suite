#include <creation/ui/SuiteSettingsPanel.h>
#include <creation/ui/CreationSuiteLogos.h>
#include <creation/ui/SuiteAiAccountDialog.h>
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

}

SuiteSettingsPanel::SuiteSettingsPanel()
    : aiProviders(creation::services::SuiteAiProviderCatalog::createDefaultCatalog())
{
    suiteLogo = creation::ui::getSuiteLogoImage(creation::ui::SuiteLogoId::suite);

    titleLabel.setText("Creation Suite", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(28.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    subTitleLabel.setText("Suite-wide storage, AI accounts, and platform controls live here.",
                          juce::dontSendNotification);
    subTitleLabel.setColour(juce::Label::textColourId, hintColour());
    addAndMakeVisible(subTitleLabel);

    configureTabButton(storageTabButton, "Storage", Tab::storage);
    configureTabButton(aiTabButton, "Suite AI Accounts", Tab::ai);
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
                       "Named accounts live here so every app in the suite can pick one to use. Each app chooses its own account and model itself - nothing is assigned to an app from this screen.");
    scrollContent.addAndMakeVisible(aiIntroLabel);

    configureSectionTitle(aiSectionLabel, "Suite AI Accounts");
    scrollContent.addAndMakeVisible(aiSectionLabel);

    accountListBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff1a2230));
    accountListBox.setColour(juce::ListBox::outlineColourId, panelOutline());
    accountListBox.setOutlineThickness(1);
    accountListBox.setRowHeight(42);
    scrollContent.addAndMakeVisible(accountListBox);

    addAccountButton.onClick = [this]
    {
        creation::ui::showAddOrEditAiAccountDialog(aiProviders, {},
            [this](bool saved, const creation::services::SuiteAiAccountSettings& account)
            {
                if (! saved)
                    return;

                aiSettings.accounts.add(account);
                selectedAccountIndex = aiSettings.accounts.size() - 1;
                refreshAiAccountUi();
                persistAiSettings();
                appendLogLine("Added suite AI account \"" + account.accountLabel + "\".");
            });
    };
    scrollContent.addAndMakeVisible(addAccountButton);

    editAccountButton.onClick = [this]
    {
        if (! juce::isPositiveAndBelow(selectedAccountIndex, aiSettings.accounts.size()))
            return;

        const auto accountIndex = selectedAccountIndex;
        creation::ui::showAddOrEditAiAccountDialog(aiProviders, aiSettings.accounts.getReference(accountIndex),
            [this, accountIndex](bool saved, const creation::services::SuiteAiAccountSettings& account)
            {
                if (! saved || ! juce::isPositiveAndBelow(accountIndex, aiSettings.accounts.size()))
                    return;

                aiSettings.accounts.getReference(accountIndex) = account;
                refreshAiAccountUi();
                persistAiSettings();
                appendLogLine("Updated suite AI account \"" + account.accountLabel + "\".");
            });
    };
    scrollContent.addAndMakeVisible(editAccountButton);

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
        persistAiSettings();
        appendLogLine("Removed a suite AI account.");
    };
    scrollContent.addAndMakeVisible(removeAccountButton);

    testAccountButton.onClick = [this]
    {
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

    configureLabel(accountSummaryLabel);
    accountSummaryLabel.setColour(juce::Label::textColourId, hintColour());
    scrollContent.addAndMakeVisible(accountSummaryLabel);

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
    setStatusText("Suite storage and AI accounts are controlled here. Each app picks its own account and model itself.");
}

SuiteSettingsPanel::~SuiteSettingsPanel()
{
#if JUCE_DEBUG
    // vfsTreeView is declared before vfsTreeRoot, so implicit member destruction would tear
    // down vfsTreeRoot (the actual TreeViewItem tree) first, leaving vfsTreeView holding a
    // dangling root-item pointer for the moment before its own destructor runs - a real crash
    // on close, not hypothetical (reported live 2026-08-13). Detach explicitly, in the right
    // order, before either member is destroyed.
    vfsTreeView.setRootItem(nullptr);
#endif
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
}

creation::services::SuiteAiSettings SuiteSettingsPanel::getAiSettings() const
{
    // This panel only ever manages the account list itself - which account/model each app uses
    // is chosen by that app, not assigned here, so appSelections/defaultAccountId simply pass
    // through untouched (whatever the apps themselves have already written into the shared store).
    return aiSettings;
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
    aiTabButton.setBounds(tabArea.removeFromLeft(tabButtonWidth + 44));
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
        accountListBox.setVisible(visible);
        addAccountButton.setVisible(visible);
        editAccountButton.setVisible(visible);
        removeAccountButton.setVisible(visible);
        testAccountButton.setVisible(visible);
        accountSummaryLabel.setVisible(visible);
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
    addAccountButton.setBounds(accountHeaderRow.removeFromLeft(96));
    accountHeaderRow.removeFromLeft(8);
    editAccountButton.setBounds(accountHeaderRow.removeFromLeft(70));
    accountHeaderRow.removeFromLeft(8);
    removeAccountButton.setBounds(accountHeaderRow.removeFromLeft(90));
    accountHeaderRow.removeFromLeft(8);
    testAccountButton.setBounds(accountHeaderRow.removeFromLeft(128));
    area.removeFromTop(10);

    accountListBox.setBounds(area.removeFromTop(6 * accountListBox.getRowHeight() + 4));
    area.removeFromTop(10);

    accountSummaryLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(14);
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
    if (aiSettings.accounts.isEmpty())
        selectedAccountIndex = -1;
    else if (! juce::isPositiveAndBelow(selectedAccountIndex, aiSettings.accounts.size()))
        selectedAccountIndex = 0;

    accountListBox.updateContent();
    if (selectedAccountIndex >= 0)
        accountListBox.selectRow(selectedAccountIndex);
    else
        accountListBox.deselectAllRows();

    removeAccountButton.setEnabled(selectedAccountIndex >= 0);
    editAccountButton.setEnabled(selectedAccountIndex >= 0);
    refreshSelectedAccountSummary();
}

int SuiteSettingsPanel::AccountListBoxModel::getNumRows()
{
    return owner.aiSettings.accounts.size();
}

void SuiteSettingsPanel::AccountListBoxModel::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (! juce::isPositiveAndBelow(rowNumber, owner.aiSettings.accounts.size()))
        return;

    if (rowIsSelected)
        g.fillAll(accentColour().withAlpha(0.16f));

    const auto& account = owner.aiSettings.accounts.getReference(rowNumber);
    auto label = account.accountLabel.trim();
    if (label.isEmpty())
        label = "Suite Account " + juce::String(rowNumber + 1);
    const auto providerDisplayName = creation::services::SuiteAiProviderRuntime::resolveProfile(account.providerId).displayName;

    auto area = juce::Rectangle<int>(0, 0, width, height).reduced(10, 2);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(15.0f).boldened());
    g.drawText(label, area.removeFromTop(height / 2), juce::Justification::centredLeft, true);

    g.setColour(hintColour());
    g.setFont(juce::Font(12.0f));
    const auto detail = providerDisplayName + "  \xe2\x80\xa2  " + juce::String(account.cachedModelIds.size()) + " model(s) cached";
    g.drawText(detail, area, juce::Justification::centredLeft, true);
}

void SuiteSettingsPanel::AccountListBoxModel::selectedRowsChanged(int lastRowSelected)
{
    owner.selectedAccountIndex = lastRowSelected;
    owner.editAccountButton.setEnabled(lastRowSelected >= 0);
    owner.removeAccountButton.setEnabled(lastRowSelected >= 0);
    owner.refreshSelectedAccountSummary();
}

void SuiteSettingsPanel::persistAiSettings()
{
    if (onApplyAiSettingsRequested)
        onApplyAiSettingsRequested(aiSettings);
}

void SuiteSettingsPanel::refreshSelectedAccountSummary()
{
    if (! juce::isPositiveAndBelow(selectedAccountIndex, aiSettings.accounts.size()))
    {
        accountSummaryLabel.setText("No AI accounts configured yet. Use + Account to add one.", juce::dontSendNotification);
        return;
    }

    const auto& account = aiSettings.accounts.getReference(selectedAccountIndex);
    const auto providerDisplayName = creation::services::SuiteAiProviderRuntime::resolveProfile(account.providerId).displayName;

    juce::String summary = providerDisplayName;
    summary << "  \xe2\x80\xa2  " << account.baseUrl;
    summary << "  \xe2\x80\xa2  " << account.cachedModelIds.size() << " model(s) cached";
    summary << "  \xe2\x80\xa2  each tool picks its own model from this account's cached list";
    accountSummaryLabel.setText(summary, juce::dontSendNotification);
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
            vfsContentViewer.setText(juce::JSON::toString(parsed, false), juce::dontSendNotification);
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
