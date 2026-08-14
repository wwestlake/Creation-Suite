#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <creation/services/SuiteAiSettings.h>
#include <creation/suite/SuiteSettings.h>

class SuiteSettingsPanel final : public juce::Component
{
public:
    SuiteSettingsPanel();
    ~SuiteSettingsPanel() override;

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
        log,
#if JUCE_DEBUG
        vfsBrowser,
#endif
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

#if JUCE_DEBUG
    // Debug-only VFS inspection tool -- see docs/Suite-VFS-Browser-Debug-Tool.md. Reads whatever
    // the VFS actually contains at runtime (real files under the configured root, plus whatever
    // entries the VFS service reports) rather than assuming any particular layout, so it stays
    // correct through future reorganizations of the VFS instead of needing to be rebuilt alongside
    // one.
    class VfsFileTreeItem final : public juce::TreeViewItem
    {
    public:
        VfsFileTreeItem(const juce::File& file, std::function<void(const juce::File&)> onSelected);
        bool mightContainSubItems() override;
        void itemOpennessChanged(bool isNowOpen) override;
        void paintItem(juce::Graphics& g, int width, int height) override;
        void itemClicked(const juce::MouseEvent&) override;

    private:
        juce::File file_;
        std::function<void(const juce::File&)> onSelected_;
    };

    // A synthetic node (not backed by a real directory) whose children are built from a flat
    // logical-path list -- used for the VFS service's own "suite/" entries, which live inside the
    // suite root project's container, not as loose OS files.
    class VfsEntryTreeItem final : public juce::TreeViewItem
    {
    public:
        VfsEntryTreeItem(juce::String displayName, juce::String fullLogicalPath, bool isLeaf,
                         std::function<void(const juce::String&)> onSelected);
        void addChildPath(const juce::StringArray& remainingSegments, const juce::String& fullLogicalPath);
        bool mightContainSubItems() override { return getNumSubItems() > 0; }
        void paintItem(juce::Graphics& g, int width, int height) override;
        void itemClicked(const juce::MouseEvent&) override;

    private:
        VfsEntryTreeItem* findOrCreateChild(const juce::String& segment);
        juce::String displayName_;
        juce::String fullLogicalPath_;
        bool isLeaf_ = false;
        std::function<void(const juce::String&)> onSelected_;
    };

    void buildVfsBrowserTree();
    void showVfsFileContent(const juce::File& file);
    void showVfsEntryContent(const juce::String& logicalPath);
    void setVfsContentText(const juce::String& title, const juce::String& rawContent, bool wasParsed, const juce::String& parseNote);
#endif

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
    void refreshSelectedAccountSummary();
    juce::String accountIdForCombo(const juce::ComboBox& comboBox) const;

    juce::Label titleLabel;
    juce::Label subTitleLabel;
    juce::Image suiteLogo;
    juce::TextButton storageTabButton { "Storage" };
    juce::TextButton aiTabButton { "AI & Routing" };
    juce::TextButton suiteLogTabButton { "Suite Log" };
#if JUCE_DEBUG
    juce::TextButton vfsBrowserTabButton { "VFS Browser" };
    juce::TreeView vfsTreeView;
    std::unique_ptr<juce::TreeViewItem> vfsTreeRoot;
    juce::Label vfsContentTitleLabel;
    juce::TextEditor vfsContentViewer;
#endif
    juce::Viewport scrollViewport;
    juce::Component scrollContent;
    juce::Label storageIntroLabel;
    juce::Label storageSectionLabel;
    PathRow suiteVfsRow;
    juce::Label aiIntroLabel;
    juce::Label aiSectionLabel;
    juce::ComboBox accountSelectorCombo;
    juce::TextButton addAccountButton { "+ Account" };
    juce::TextButton editAccountButton { "Edit" };
    juce::TextButton removeAccountButton { "Remove" };
    juce::TextButton testAccountButton { "Test Account" };
    // Read-only summary of the selected account (provider/model/cached-model-count) - account
    // details are only ever edited through the Add/Edit Account dialog now, not inline here.
    juce::Label accountSummaryLabel;
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
    creation::suite::SuiteSettings currentSettings;
};
