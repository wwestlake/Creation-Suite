#include <creation/ui/SuiteAssetManagerPanel.h>

#include <creation/assets/AssetTypes.h>
#include <creation/suite/SuiteSettings.h>
#include <creation/suite/SuiteStoragePaths.h>

namespace creation::ui
{
namespace
{
const juce::StringArray& codeExtensions()
{
    static const juce::StringArray extensions { ".frust", ".cpp", ".h", ".hpp", ".py", ".js", ".lua" };
    return extensions;
}

class DirectoriesOnlyFilter final : public juce::FileFilter
{
public:
    DirectoriesOnlyFilter() : juce::FileFilter("Folders") {}

    bool isFileSuitable(const juce::File&) const override { return false; }

    bool isDirectorySuitable(const juce::File& file) const override
    {
        return ! file.getFileName().startsWithChar('.');
    }
};

class SuiteAssetDetailsPanel final : public juce::Component
{
public:
    std::function<void()> onOpenRequested;
    std::function<void()> onStopRequested;
    std::function<void()> onPlaceRequested;
    std::function<void()> onExportRequested;

    SuiteAssetDetailsPanel()
    {
        headerLabel.setText("Details", juce::dontSendNotification);
        headerLabel.setFont(juce::Font(16.0f).boldened());
        headerLabel.setColour(juce::Label::textColourId, juce::Colour(0xff74caff));
        addAndMakeVisible(headerLabel);

        nameLabel.setFont(juce::Font(18.0f).boldened());
        nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(nameLabel);

        metaLabel.setColour(juce::Label::textColourId, juce::Colour(0xffb8c4d5));
        metaLabel.setJustificationType(juce::Justification::topLeft);
        addAndMakeVisible(metaLabel);

        statusLabel.setJustificationType(juce::Justification::topLeft);
        addAndMakeVisible(statusLabel);

        openButton.setButtonText("Open / Preview");
        openButton.onClick = [this]
        {
            if (onOpenRequested)
                onOpenRequested();
        };
        addAndMakeVisible(openButton);

        stopButton.setButtonText("Stop");
        stopButton.onClick = [this]
        {
            if (onStopRequested)
                onStopRequested();
        };
        addAndMakeVisible(stopButton);

        placeButton.setButtonText("Place on Track");
        placeButton.onClick = [this]
        {
            if (onPlaceRequested)
                onPlaceRequested();
        };
        addAndMakeVisible(placeButton);

        exportButton.setButtonText("Export");
        exportButton.onClick = [this]
        {
            if (onExportRequested)
                onExportRequested();
        };
        addAndMakeVisible(exportButton);

        showNothingSelected();
    }

    void showNothingSelected()
    {
        nameLabel.setText("No selection", juce::dontSendNotification);
        metaLabel.setText({}, juce::dontSendNotification);
        statusLabel.setText({}, juce::dontSendNotification);
        openButton.setEnabled(false);
        stopButton.setEnabled(false);
        placeButton.setEnabled(false);
        exportButton.setEnabled(false);
    }

    void setFile(const juce::File& file, const SuiteAssetManagerCapability& capability)
    {
        if (! file.exists())
        {
            showNothingSelected();
            return;
        }

        nameLabel.setText(file.getFileName(), juce::dontSendNotification);

        if (file.isDirectory())
        {
            metaLabel.setText("Folder", juce::dontSendNotification);
            statusLabel.setText({}, juce::dontSendNotification);
            openButton.setEnabled(false);
            stopButton.setEnabled(false);
            placeButton.setEnabled(false);
            exportButton.setEnabled(false);
            return;
        }

        auto extension = file.getFileExtension();
        juce::String meta;
        meta << "Type: " << (extension.isNotEmpty() ? extension : juce::String("(none)")) << juce::newLine;
        meta << "Size: " << juce::File::descriptionOfSizeInBytes(file.getSize()) << juce::newLine;
        meta << "Modified: " << file.getLastModificationTime().toString(true, true);
        metaLabel.setText(meta, juce::dontSendNotification);

        if (codeExtensions().contains(extension.toLowerCase()))
        {
            if (capability.canRun(extension))
            {
                statusLabel.setText("Runnable here (" + capability.hostAppDisplayName + ")", juce::dontSendNotification);
                statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff6fe89a));
            }
            else if (capability.canParse(extension))
            {
                statusLabel.setText("Viewable here, not runnable (" + capability.hostAppDisplayName + ")",
                                    juce::dontSendNotification);
                statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe8c96f));
            }
            else
            {
                statusLabel.setText("Not supported by " + capability.hostAppDisplayName, juce::dontSendNotification);
                statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe86f6f));
            }
        }
        else
        {
            statusLabel.setText({}, juce::dontSendNotification);
        }

        openButton.setEnabled(false);
        stopButton.setEnabled(false);
        placeButton.setEnabled(false);
        exportButton.setEnabled(false);
    }

    void setAsset(const creation::assets::AssetDescriptor& asset, const SuiteAssetManagerCapability& capability)
    {
        nameLabel.setText(asset.displayName.isNotEmpty() ? asset.displayName : asset.logicalPath, juce::dontSendNotification);

        auto extension = juce::File(asset.logicalPath).getFileExtension();
        juce::String meta;
        meta << "Kind: " << creation::assets::toDisplayName(asset.kind) << juce::newLine;
        meta << "Path: " << asset.logicalPath << juce::newLine;
        meta << "Size: " << juce::File::descriptionOfSizeInBytes(asset.fileSizeBytes) << juce::newLine;
        meta << "Modified: " << asset.modifiedAt.toString(true, true);
        if (capability.describeProjectAsset)
            meta << capability.describeProjectAsset(asset);
        metaLabel.setText(meta, juce::dontSendNotification);

        if (capability.canRun(extension))
        {
            statusLabel.setText("Runnable here (" + capability.hostAppDisplayName + ")", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff6fe89a));
        }
        else if (capability.canParse(extension))
        {
            statusLabel.setText("Viewable here, not runnable (" + capability.hostAppDisplayName + ")",
                                juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe8c96f));
        }
        else
        {
            statusLabel.setText("Project asset", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff74caff));
        }

        openButton.setEnabled(static_cast<bool>(capability.openProjectAsset));
        stopButton.setEnabled(static_cast<bool>(capability.stopProjectAsset));
        placeButton.setEnabled(static_cast<bool>(capability.placeProjectAsset));
        exportButton.setEnabled(static_cast<bool>(capability.exportProjectAsset));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(12);
        headerLabel.setBounds(area.removeFromTop(24));
        area.removeFromTop(8);
        nameLabel.setBounds(area.removeFromTop(28));
        area.removeFromTop(8);
        metaLabel.setBounds(area.removeFromTop(96));
        area.removeFromTop(8);
        statusLabel.setBounds(area.removeFromTop(24));
        area.removeFromTop(12);
        auto actions = area.removeFromTop(34);
        openButton.setBounds(actions.removeFromLeft(110));
        actions.removeFromLeft(8);
        stopButton.setBounds(actions.removeFromLeft(80));
        actions.removeFromLeft(8);
        placeButton.setBounds(actions.removeFromLeft(120));
        actions.removeFromLeft(8);
        exportButton.setBounds(actions.removeFromLeft(90));
    }

private:
    juce::Label headerLabel;
    juce::Label nameLabel;
    juce::Label metaLabel;
    juce::Label statusLabel;
    juce::TextButton openButton;
    juce::TextButton stopButton;
    juce::TextButton placeButton;
    juce::TextButton exportButton;
};

class SuiteFilesystemListModel final : public juce::ListBoxModel
{
public:
    std::function<void(const juce::File&)> onFileSelected;

    void setFiles(const juce::Array<juce::File>& newFiles)
    {
        files = newFiles;
        files.sort();
    }

    int getNumRows() override { return files.size(); }

    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (rowNumber < 0 || rowNumber >= files.size())
            return;

        if (rowIsSelected)
            g.fillAll(juce::Colour(0xff273e5e));
        else if (rowNumber % 2 == 0)
            g.fillAll(juce::Colour(0xff141c26));

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(14.0f));
        g.drawText(files[rowNumber].getFileName(), 8, 0, width - 16, height, juce::Justification::centredLeft);
    }

    void selectedRowsChanged(int lastRowSelected) override
    {
        if (onFileSelected && juce::isPositiveAndBelow(lastRowSelected, files.size()))
            onFileSelected(files[lastRowSelected]);
    }

private:
    juce::Array<juce::File> files;
};

class SuiteProjectAssetListModel final : public juce::ListBoxModel
{
public:
    std::function<void(const creation::assets::AssetDescriptor&)> onAssetSelected;

    void setAssets(const juce::Array<creation::assets::AssetDescriptor>& newAssets)
    {
        assets = newAssets;
        std::sort(assets.begin(), assets.end(), [](const auto& left, const auto& right)
        {
            if (left.modifiedAt == right.modifiedAt)
                return left.displayName.compareIgnoreCase(right.displayName) < 0;
            return left.modifiedAt > right.modifiedAt;
        });
    }

    int getNumRows() override { return assets.size(); }

    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (rowNumber < 0 || rowNumber >= assets.size())
            return;

        if (rowIsSelected)
            g.fillAll(juce::Colour(0xff273e5e));
        else if (rowNumber % 2 == 0)
            g.fillAll(juce::Colour(0xff141c26));

        const auto& asset = assets.getReference(rowNumber);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(14.0f));
        g.drawText(asset.displayName.isNotEmpty() ? asset.displayName : asset.logicalPath,
                   8, 0, width - 16, height, juce::Justification::centredLeft);
    }

    void selectedRowsChanged(int lastRowSelected) override
    {
        if (onAssetSelected && juce::isPositiveAndBelow(lastRowSelected, assets.size()))
            onAssetSelected(assets.getReference(lastRowSelected));
    }

private:
    juce::Array<creation::assets::AssetDescriptor> assets;
};

class SuiteAssetExplorerTab final : public juce::Component,
                                    public juce::FileBrowserListener
{
public:
    explicit SuiteAssetExplorerTab(SuiteAssetManagerCapability capabilityToUse)
        : capability(std::move(capabilityToUse)),
          scanThread("SuiteAssetManagerScan")
    {
        scanThread.startThread();

        if (capability.enumerateProjectAssets)
        {
            refreshButton.setButtonText("Refresh");
            refreshButton.onClick = [this] { refreshProjectAssetList(); };
            addAndMakeVisible(refreshButton);

            projectAssetListModel.onAssetSelected = [this](const creation::assets::AssetDescriptor& asset)
            {
                selectedAsset = asset;
                detailsPanel.setAsset(asset, capability);
            };
            projectAssetList.setModel(&projectAssetListModel);
            projectAssetList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff121a24));
            projectAssetList.setColour(juce::ListBox::outlineColourId, juce::Colour(0xff253549));
            projectAssetList.setRowHeight(24);
            addAndMakeVisible(projectAssetList);

            refreshProjectAssetList();
        }
        else
        {
            auto rootDirectory = resolveRootDirectory();
            directoryList = std::make_unique<juce::DirectoryContentsList>(&directoriesFilter, scanThread);
            directoryList->setDirectory(rootDirectory, true, true);

            fileTree = std::make_unique<juce::FileTreeComponent>(*directoryList);
            fileTree->addListener(this);
            addAndMakeVisible(*fileTree);

            filesystemListModel.onFileSelected = [this](const juce::File& file) { detailsPanel.setFile(file, capability); };
            filesystemList.setModel(&filesystemListModel);
            filesystemList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff121a24));
            filesystemList.setColour(juce::ListBox::outlineColourId, juce::Colour(0xff253549));
            filesystemList.setRowHeight(24);
            addAndMakeVisible(filesystemList);

            refreshFileList(rootDirectory);
        }

        addAndMakeVisible(detailsPanel);
        detailsPanel.onOpenRequested = [this]
        {
            if (selectedAsset.has_value() && capability.openProjectAsset)
                capability.openProjectAsset(*selectedAsset);
        };
        detailsPanel.onStopRequested = [this]
        {
            if (selectedAsset.has_value() && capability.stopProjectAsset)
                capability.stopProjectAsset(*selectedAsset);
        };
        detailsPanel.onPlaceRequested = [this]
        {
            if (selectedAsset.has_value() && capability.placeProjectAsset)
                capability.placeProjectAsset(*selectedAsset);
        };
        detailsPanel.onExportRequested = [this]
        {
            if (selectedAsset.has_value() && capability.exportProjectAsset)
                capability.exportProjectAsset(*selectedAsset);
        };
    }

    ~SuiteAssetExplorerTab() override
    {
        if (fileTree != nullptr)
            fileTree->removeListener(this);
        scanThread.stopThread(2000);
    }

    void resized() override
    {
        auto area = getLocalBounds();

        if (capability.enumerateProjectAssets)
        {
            auto header = area.removeFromTop(36);
            refreshButton.setBounds(header.removeFromRight(120).reduced(4, 4));
            projectAssetList.setBounds(area.removeFromLeft(area.getWidth() / 2));
        }
        else
        {
            fileTree->setBounds(area.removeFromLeft(area.getWidth() / 3));
            filesystemList.setBounds(area.removeFromLeft(area.getWidth() / 2));
        }

        detailsPanel.setBounds(area);
    }

    void selectionChanged() override
    {
        if (fileTree == nullptr)
            return;

        auto selected = fileTree->getSelectedFile();
        if (selected.isDirectory())
            refreshFileList(selected);
    }

    void fileClicked(const juce::File&, const juce::MouseEvent&) override {}
    void fileDoubleClicked(const juce::File&) override {}
    void browserRootChanged(const juce::File&) override {}

private:
    juce::File resolveRootDirectory() const
    {
        creation::suite::SuiteSettingsStore store;
        juce::String errorMessage;
        auto settings = store.load(errorMessage);

        if (errorMessage.isEmpty())
        {
            auto root = capability.appDomain != creation::assets::SuiteAppDomain::unknown
                            ? creation::suite::getAppProjectsDirectory(settings, capability.appDomain)
                            : creation::suite::getSuiteRootDirectory(settings);

            if (! root.exists())
                root.createDirectory();

            if (root.isDirectory())
                return root;
        }

        return juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    }

    void refreshFileList(const juce::File& directory)
    {
        juce::Array<juce::File> files;
        directory.findChildFiles(files, juce::File::findFiles, false);
        filesystemListModel.setFiles(files);
        filesystemList.updateContent();
        filesystemList.repaint();
    }

    void refreshProjectAssetList()
    {
        auto assets = capability.enumerateProjectAssets();
        projectAssetListModel.setAssets(assets);
        projectAssetList.updateContent();
        projectAssetList.repaint();
        if (assets.isEmpty())
        {
            selectedAsset.reset();
            detailsPanel.showNothingSelected();
        }
    }

    SuiteAssetManagerCapability capability;
    juce::TimeSliceThread scanThread;
    DirectoriesOnlyFilter directoriesFilter;
    std::unique_ptr<juce::DirectoryContentsList> directoryList;
    std::unique_ptr<juce::FileTreeComponent> fileTree;
    juce::ListBox filesystemList;
    SuiteFilesystemListModel filesystemListModel;
    juce::TextButton refreshButton;
    juce::ListBox projectAssetList;
    SuiteProjectAssetListModel projectAssetListModel;
    SuiteAssetDetailsPanel detailsPanel;
    std::optional<creation::assets::AssetDescriptor> selectedAsset;
};

class SuiteAssetManagerPlaceholderTab final : public juce::Component
{
public:
    explicit SuiteAssetManagerPlaceholderTab(const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, juce::Colour(0xff8ba1bc));
        addAndMakeVisible(label);
    }

    void resized() override { label.setBounds(getLocalBounds()); }

private:
    juce::Label label;
};
}

SuiteAssetManagerPanel::SuiteAssetManagerPanel(SuiteAssetManagerCapability capability)
{
    tabs.addTab("Project Assets", juce::Colour(0xff10161f), new SuiteAssetExplorerTab(std::move(capability)), true);
    tabs.addTab("Asset Store",
                juce::Colour(0xff10161f),
                new SuiteAssetManagerPlaceholderTab("Coming soon -- LagDaemon.com asset store"),
                true);
    tabs.addTab("Shared Assets",
                juce::Colour(0xff10161f),
                new SuiteAssetManagerPlaceholderTab("Coming soon -- suite VFS shared assets"),
                true);
    addAndMakeVisible(tabs);
}

SuiteAssetManagerPanel::~SuiteAssetManagerPanel() = default;

void SuiteAssetManagerPanel::resized()
{
    tabs.setBounds(getLocalBounds());
}
}
