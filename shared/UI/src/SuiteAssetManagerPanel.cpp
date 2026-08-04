#include <creation/ui/SuiteAssetManagerPanel.h>

#include <creation/suite/SuiteSettings.h>
#include <creation/suite/SuiteStoragePaths.h>

namespace creation::ui
{
namespace
{
// A small, fixed set of extensions treated as "code" for compile/run gating
// purposes. Anything outside this set is just a regular file -- no gating
// status is shown for it.
const juce::StringArray& codeExtensions()
{
    static const juce::StringArray extensions { ".cel", ".cpp", ".h", ".hpp", ".py", ".js", ".lua" };
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

class SuiteAssetFileListModel final : public juce::ListBoxModel
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

    const juce::File& getFile(int row) const { return files.getReference(row); }

private:
    juce::Array<juce::File> files;
};

class SuiteAssetDetailsPanel final : public juce::Component
{
public:
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

        showNothingSelected();
    }

    void showNothingSelected()
    {
        nameLabel.setText("No selection", juce::dontSendNotification);
        metaLabel.setText({}, juce::dontSendNotification);
        statusLabel.setText({}, juce::dontSendNotification);
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
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(12);
        headerLabel.setBounds(area.removeFromTop(24));
        area.removeFromTop(8);
        nameLabel.setBounds(area.removeFromTop(28));
        area.removeFromTop(8);
        metaLabel.setBounds(area.removeFromTop(80));
        area.removeFromTop(8);
        statusLabel.setBounds(area.removeFromTop(24));
    }

private:
    juce::Label headerLabel;
    juce::Label nameLabel;
    juce::Label metaLabel;
    juce::Label statusLabel;
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

        auto rootDirectory = resolveRootDirectory();
        directoryList = std::make_unique<juce::DirectoryContentsList>(&directoriesFilter, scanThread);
        directoryList->setDirectory(rootDirectory, true, true);

        fileTree = std::make_unique<juce::FileTreeComponent>(*directoryList);
        fileTree->addListener(this);
        addAndMakeVisible(*fileTree);

        fileListModel.onFileSelected = [this](const juce::File& file) { showFileDetails(file); };
        fileList.setModel(&fileListModel);
        fileList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff121a24));
        fileList.setColour(juce::ListBox::outlineColourId, juce::Colour(0xff253549));
        fileList.setRowHeight(24);
        addAndMakeVisible(fileList);

        addAndMakeVisible(detailsPanel);

        refreshFileList(rootDirectory);
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
        fileTree->setBounds(area.removeFromLeft(area.getWidth() / 3));
        fileList.setBounds(area.removeFromLeft(area.getWidth() / 2));
        detailsPanel.setBounds(area);
    }

    // juce::FileBrowserListener
    void selectionChanged() override
    {
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
            auto root = creation::suite::getProjectContainerDirectory(settings);
            if (root.isDirectory())
                return root;
        }

        return juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    }

    void refreshFileList(const juce::File& directory)
    {
        juce::Array<juce::File> files;
        directory.findChildFiles(files, juce::File::findFiles, false);
        fileListModel.setFiles(files);
        fileList.updateContent();
        fileList.repaint();
    }

    void showFileDetails(const juce::File& file)
    {
        detailsPanel.setFile(file, capability);
    }

    SuiteAssetManagerCapability capability;
    juce::TimeSliceThread scanThread;
    DirectoriesOnlyFilter directoriesFilter;
    std::unique_ptr<juce::DirectoryContentsList> directoryList;
    std::unique_ptr<juce::FileTreeComponent> fileTree;
    juce::ListBox fileList;
    SuiteAssetFileListModel fileListModel;
    SuiteAssetDetailsPanel detailsPanel;
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
    tabs.addTab("Files", juce::Colour(0xff10161f), new SuiteAssetExplorerTab(std::move(capability)), true);
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
