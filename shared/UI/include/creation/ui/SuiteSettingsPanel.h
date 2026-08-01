#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <creation/services/SuiteAiSettings.h>
#include <creation/suite/SuiteSettings.h>

class SuiteSettingsPanel final : public juce::Component
{
public:
    SuiteSettingsPanel();

    std::function<void(const juce::String& fieldId)> onBrowseRequested;
    std::function<void(const creation::suite::SuiteSettings& settings)> onApplyRequested;
    std::function<void(const creation::services::SuiteAiSettings& settings)> onApplyAiSettingsRequested;
    std::function<void(const creation::services::SuiteAiResolvedRuntimeSettings& settings)> onTestAiAccountRequested;
    std::function<void()> onReadEulaRequested;

    void setSettings(const creation::suite::SuiteSettings& settings);
    creation::suite::SuiteSettings getSettings() const;
    void setAiSettings(const creation::services::SuiteAiSettings& settings);
    creation::services::SuiteAiSettings getAiSettings() const;
    void setStatusText(const juce::String& text);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    enum class Tab
    {
        storage,
        ai,
        log
    };

    struct AppSelectionRow
    {
        juce::Label label;
        juce::ComboBox accountCombo;
        juce::TextEditor modelOverrideEditor;
    };

    struct PathRow
    {
        juce::String id;
        juce::Label label;
        juce::TextEditor editor;
        juce::TextButton browseButton { "Browse" };
    };

    void configureRow(PathRow& row, const juce::String& id, const juce::String& labelText);
    void layoutRow(PathRow& row, juce::Rectangle<int>& area);
    void configureTabButton(juce::TextButton& button, const juce::String& text, Tab tab);
    void selectTab(Tab tab);
    void refreshTabButtons();
    void updateScrollContentLayout();
    void layoutStorageTab(juce::Rectangle<int>& area);
    void layoutAiTab(juce::Rectangle<int>& area);
    void layoutLogTab(juce::Rectangle<int>& area);
    void appendLogLine(const juce::String& text);
    void refreshAiAccountUi();
    void refreshAccountSelectors();
    void pushCurrentEditorToSelectedAccount();
    void pullSelectedAccountIntoEditor();
    juce::String accountIdForCombo(const juce::ComboBox& comboBox) const;

    juce::Label titleLabel;
    juce::Label subTitleLabel;
    juce::Image suiteLogo;
    juce::TextButton storageTabButton { "Storage" };
    juce::TextButton aiTabButton { "AI & Routing" };
    juce::TextButton suiteLogTabButton { "Suite Log" };
    juce::Viewport scrollViewport;
    juce::Component scrollContent;
    juce::Label storageIntroLabel;
    juce::Label storageSectionLabel;
    PathRow suiteVfsRow;
    PathRow sharedResourcesRow;
    PathRow projectContainersRow;
    PathRow cacheRootRow;
    PathRow materializedFilesRow;
    PathRow exportsRootRow;
    PathRow stationProjectsRow;
    PathRow engineProjectsRow;
    PathRow movieProjectsRow;
    PathRow liveProjectsRow;
    juce::Label aiIntroLabel;
    juce::Label aiSectionLabel;
    juce::ComboBox accountSelectorCombo;
    juce::TextButton addAccountButton { "+ Account" };
    juce::TextButton removeAccountButton { "Remove" };
    juce::TextButton testAccountButton { "Test Account" };
    juce::Label providerLabel;
    juce::ComboBox providerCombo;
    juce::Label accountLabelLabel;
    juce::TextEditor accountLabelEditor;
    juce::Label endpointLabel;
    juce::TextEditor endpointEditor;
    juce::Label modelLabel;
    juce::TextEditor modelEditor;
    juce::Label apiKeyLabel;
    juce::TextEditor apiKeyEditor;
    juce::Label defaultAccountLabel;
    juce::ComboBox defaultAccountCombo;
    AppSelectionRow stationSelectionRow;
    AppSelectionRow engineSelectionRow;
    AppSelectionRow movieSelectionRow;
    AppSelectionRow liveSelectionRow;
    juce::Label logSectionLabel;
    juce::Label logIntroLabel;
    juce::TextEditor suiteLogEditor;
    juce::TextButton applyButton { "Apply Suite Settings" };
    juce::TextButton eulaButton { "Read EULA" };
    juce::Label statusLabel;
    creation::services::SuiteAiSettings aiSettings;
    juce::Array<creation::services::SuiteAiProviderPreset> aiProviders;
    int selectedAccountIndex = -1;
    Tab selectedTab = Tab::storage;
};
