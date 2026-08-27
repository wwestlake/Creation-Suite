#include <creation/ui/SuiteShellController.h>
#include <creation/ui/SuiteAssetManagerPanel.h>
#include <creation/ui/SuiteEulaPanel.h>

#include <creation/suite/SuiteStoragePaths.h>

namespace
{
juce::File getDefaultSuiteStorageBrowseRoot()
{
    auto roamingAppData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto appDataRoot = roamingAppData.getParentDirectory();
    auto localAppData = appDataRoot.getChildFile("Local");
    if (localAppData.isDirectory())
        return localAppData.getChildFile("Creation Suite").getChildFile("Data");

    return roamingAppData.getChildFile("Creation Suite").getChildFile("Data");
}

juce::String suiteAuthAppSlug(creation::assets::SuiteAppDomain domain)
{
    switch (domain)
    {
        case creation::assets::SuiteAppDomain::station: return "creative-workstation";
        case creation::assets::SuiteAppDomain::engine: return "creation-engine";
        case creation::assets::SuiteAppDomain::movie: return "creation-movie";
        case creation::assets::SuiteAppDomain::live: return "creation-live";
        case creation::assets::SuiteAppDomain::texture: return "creation-texture";
        case creation::assets::SuiteAppDomain::modeler: return "creation-modeler";
        case creation::assets::SuiteAppDomain::developer: return "creation-developer";
        case creation::assets::SuiteAppDomain::unknown: break;
    }

    return "creation-suite";
}

juce::String profileDetailText(const creation::ui::SuiteDesktopAuthSession::SessionData& session)
{
    if (session.user.role.isNotEmpty())
        return session.user.email + " - " + session.user.role;
    return session.user.email;
}

class ManagedDocumentWindow final : public juce::DocumentWindow
{
public:
    ManagedDocumentWindow(const juce::String& title,
                          juce::Colour backgroundColour,
                          int buttons,
                          std::function<void()> onCloseCallback)
        : juce::DocumentWindow(title, backgroundColour, buttons),
          onClose(std::move(onCloseCallback))
    {
    }

    void closeButtonPressed() override
    {
        setVisible(false);
        if (onClose)
            onClose();
    }

private:
    std::function<void()> onClose;
};

class SuiteProjectBrowserPanel final : public juce::Component,
                                       public juce::ListBoxModel
{
public:
    explicit SuiteProjectBrowserPanel(creation::assets::SuiteAppDomain currentDomainToUse)
        : currentDomain(currentDomainToUse)
    {
        titleLabel.setText("Creation Suite Project Manager", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(22.0f).boldened());
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(titleLabel);

        subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ba1bc));
        addAndMakeVisible(subtitleLabel);

        searchEditor.setTextToShowWhenEmpty("Search projects...", juce::Colour(0xff677b93));
        searchEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff16202c));
        searchEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2d3e54));
        searchEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        searchEditor.onTextChange = [this] { filterProjects(); };
        addAndMakeVisible(searchEditor);

        domainCombo.addItem("All Applications", 1);
        domainCombo.addItem("Creation Station", 2);
        domainCombo.addItem("Creation Engine", 3);
        domainCombo.addItem("Creation Movie", 4);
        domainCombo.addItem("Creation Live", 5);
        domainCombo.addItem("Creation Texture", 6);
        domainCombo.addItem("Creation Modeler", 7);
        domainCombo.addItem("Creation Developer", 8);
        domainCombo.setSelectedId(1, juce::dontSendNotification);
        domainCombo.onChange = [this] { filterProjects(); };
        addAndMakeVisible(domainCombo);

        refreshButton.setButtonText("Refresh List");
        refreshButton.onClick = [this] { refresh(); };
        addAndMakeVisible(refreshButton);

        newProjectButton.setButtonText("New Project...");
        newProjectButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2b7a4b));
        newProjectButton.onClick = [this] { beginCreateProject(); };
        addAndMakeVisible(newProjectButton);

        listBox.setModel(this);
        listBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff121a24));
        listBox.setColour(juce::ListBox::outlineColourId, juce::Colour(0xff253549));
        listBox.setRowHeight(48);
        addAndMakeVisible(listBox);

        detailsHeaderLabel.setText("Project Inspector", juce::dontSendNotification);
        detailsHeaderLabel.setFont(juce::Font(18.0f).boldened());
        detailsHeaderLabel.setColour(juce::Label::textColourId, juce::Colour(0xff74caff));
        addAndMakeVisible(detailsHeaderLabel);

        detailsNameLabel.setFont(juce::Font(20.0f).boldened());
        detailsNameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(detailsNameLabel);

        detailsMetaText.setReadOnly(true);
        detailsMetaText.setMultiLine(true, true);
        detailsMetaText.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff121922));
        detailsMetaText.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff233346));
        detailsMetaText.setColour(juce::TextEditor::textColourId, juce::Colour(0xffd0dfef));
        addAndMakeVisible(detailsMetaText);

        loadButton.setButtonText("Open Project");
        loadButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2965a8));
        loadButton.onClick = [this]
        {
            if (auto* selected = getSelectedProject())
                if (onProjectOpenRequested)
                    onProjectOpenRequested(selected->projectId);
        };
        addAndMakeVisible(loadButton);

        renameButton.setButtonText("Rename...");
        renameButton.onClick = [this] { renameSelectedProject(); };
        addAndMakeVisible(renameButton);

        duplicateButton.setButtonText("Clone...");
        duplicateButton.onClick = [this] { cloneSelectedProject(); };
        addAndMakeVisible(duplicateButton);

        purgeCacheButton.setButtonText("Purge Cache");
        purgeCacheButton.setTooltip("Frees temporary materialized asset cache files for this project container");
        purgeCacheButton.onClick = [this] { purgeSelectedProjectCache(); };
        addAndMakeVisible(purgeCacheButton);

        deleteButton.setButtonText("Delete Project");
        deleteButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff8a2323));
        deleteButton.setTooltip("Permanently deletes the selected project's folder from the suite VFS");
        deleteButton.onClick = [this] { confirmAndDeleteSelectedProject(); };
        addAndMakeVisible(deleteButton);

        refresh();
    }

    std::function<void(const juce::String&)> onProjectOpenRequested;

    void refresh()
    {
        creation::suite::SuiteSettingsStore store;
        juce::String errorMessage;
        loadedSettings = store.load(errorMessage);
        if (errorMessage.isNotEmpty())
        {
            subtitleLabel.setText("Could not load suite settings.", juce::dontSendNotification);
            return;
        }

        auto root = creation::suite::getProjectContainerDirectory(loadedSettings);
        subtitleLabel.setText("Suite Storage: " + root.getFullPathName(), juce::dontSendNotification);

        juce::String listError;
        allProjects = creation::assets::ProjectContainerService::listAllProjects(loadedSettings, listError);
        filterProjects();
    }

    void filterProjects()
    {
        filteredProjects.clearQuick();
        auto query = searchEditor.getText().trim().toLowerCase();
        auto selectedDomainId = domainCombo.getSelectedId();

        for (const auto& project : allProjects)
        {
            if (selectedDomainId > 1)
            {
                const auto targetDomain = [selectedDomainId]
                {
                    switch (selectedDomainId)
                    {
                        case 2: return creation::assets::SuiteAppDomain::station;
                        case 3: return creation::assets::SuiteAppDomain::engine;
                        case 4: return creation::assets::SuiteAppDomain::movie;
                        case 5: return creation::assets::SuiteAppDomain::live;
                        case 6: return creation::assets::SuiteAppDomain::texture;
                        case 7: return creation::assets::SuiteAppDomain::modeler;
                        case 8: return creation::assets::SuiteAppDomain::developer;
                        default: return creation::assets::SuiteAppDomain::unknown;
                    }
                }();
                if (project.manifest.appDomain != targetDomain)
                    continue;
            }

            if (query.isNotEmpty())
            {
                auto name = project.manifest.projectName.toLowerCase();
                if (! name.contains(query))
                    continue;
            }

            filteredProjects.add(project);
        }

        listBox.updateContent();
        listBox.repaint();
        updateSelectedProjectDetails();
    }

    int getNumRows() override { return filteredProjects.size(); }

    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (rowIsSelected)
            g.fillAll(juce::Colour(0xff273e5e));
        else if (rowNumber % 2 == 0)
            g.fillAll(juce::Colour(0xff161f2c));
        else
            g.fillAll(juce::Colour(0xff1a2434));

        if (! juce::isPositiveAndBelow(rowNumber, filteredProjects.size()))
            return;

        const auto& project = filteredProjects[rowNumber];

        auto bounds = juce::Rectangle<int>(0, 0, width, height).reduced(10, 4);
        auto domainBadge = creation::assets::toDisplayName(project.manifest.appDomain);
        
        g.setColour(rowIsSelected ? juce::Colour(0xff74caff) : juce::Colour(0xff364e6d));
        g.drawRoundedRectangle(bounds.toFloat(), 4.0f, 1.0f);

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(15.0f).boldened());
        g.drawText(project.manifest.projectName, 14, 4, width - 200, 22, juce::Justification::centredLeft, true);

        g.setColour(juce::Colour(0xff8da3c0));
        g.setFont(12.0f);
        auto sizeMb = juce::String(project.totalSizeBytes / 1024.0 / 1024.0, 1) + " MB";
        g.drawText(domainBadge + "  |  " + sizeMb + "  |  Rev " + juce::String(project.manifest.revision),
                   14, 24, width - 200, 18, juce::Justification::centredLeft, true);

        auto modTime = project.manifest.modifiedAt.formatted("%Y-%m-%d %H:%M");
        g.drawText(modTime, width - 185, 0, 170, height, juce::Justification::centredRight, true);
    }

    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override
    {
        if (juce::isPositiveAndBelow(row, filteredProjects.size()))
            if (onProjectOpenRequested)
                onProjectOpenRequested(filteredProjects[row].projectId);
    }

    void selectedRowsChanged(int lastRowSelected) override
    {
        juce::ignoreUnused(lastRowSelected);
        updateSelectedProjectDetails();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(18);
        auto header = area.removeFromTop(32);
        titleLabel.setBounds(header.removeFromLeft(360));
        refreshButton.setBounds(header.removeFromRight(100));
        header.removeFromRight(8);
        newProjectButton.setBounds(header.removeFromRight(120));

        area.removeFromTop(4);
        subtitleLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(10);

        auto body = area;
        auto leftPane = body.removeFromLeft(440);
        body.removeFromLeft(16);
        auto rightPane = body;

        auto filterRow = leftPane.removeFromTop(28);
        searchEditor.setBounds(filterRow.removeFromLeft(250));
        filterRow.removeFromLeft(8);
        domainCombo.setBounds(filterRow);

        leftPane.removeFromTop(8);
        listBox.setBounds(leftPane);

        detailsHeaderLabel.setBounds(rightPane.removeFromTop(24));
        detailsNameLabel.setBounds(rightPane.removeFromTop(28));
        rightPane.removeFromTop(8);

        auto actionsFooter = rightPane.removeFromBottom(42);
        loadButton.setBounds(actionsFooter.removeFromLeft(120));
        actionsFooter.removeFromLeft(8);
        renameButton.setBounds(actionsFooter.removeFromLeft(90));
        actionsFooter.removeFromLeft(8);
        duplicateButton.setBounds(actionsFooter.removeFromLeft(90));
        actionsFooter.removeFromLeft(8);
        purgeCacheButton.setBounds(actionsFooter.removeFromLeft(105));
        actionsFooter.removeFromLeft(8);
        deleteButton.setBounds(actionsFooter.removeFromLeft(115));

        rightPane.removeFromBottom(10);
        detailsMetaText.setBounds(rightPane);
    }

private:
    const creation::assets::ProjectContainerService::ProjectSummary* getSelectedProject() const
    {
        auto row = listBox.getSelectedRow();
        if (juce::isPositiveAndBelow(row, filteredProjects.size()))
            return &filteredProjects.getReference(row);
        return nullptr;
    }

    void updateSelectedProjectDetails()
    {
        auto* selected = getSelectedProject();
        if (selected == nullptr)
        {
            detailsNameLabel.setText("No project selected", juce::dontSendNotification);
            detailsMetaText.setText("Select a project container from the list on the left to view detailed metadata, asset manifests, and maintenance controls.");
            loadButton.setEnabled(false);
            renameButton.setEnabled(false);
            duplicateButton.setEnabled(false);
            purgeCacheButton.setEnabled(false);
            deleteButton.setEnabled(false);
            return;
        }

        loadButton.setEnabled(true);
        renameButton.setEnabled(true);
        duplicateButton.setEnabled(true);
        purgeCacheButton.setEnabled(true);
        deleteButton.setEnabled(true);

        detailsNameLabel.setText(selected->manifest.projectName, juce::dontSendNotification);

        juce::String text;
        text << "Application Domain: " << creation::assets::toDisplayName(selected->manifest.appDomain) << "\n";
        text << "Project UUID: " << selected->manifest.projectId << "\n";
        text << "Revision Number: " << selected->manifest.revision << "\n";
        text << "Project Size: " << juce::String(selected->totalSizeBytes / 1024.0 / 1024.0, 2) << " MB\n";
        text << "Created Date: " << selected->manifest.createdAt.formatted("%Y-%m-%d %H:%M:%S") << "\n";
        text << "Last Modified: " << selected->manifest.modifiedAt.formatted("%Y-%m-%d %H:%M:%S") << "\n";
        text << "Created With Suite: v" << selected->manifest.createdWithSuiteVersion << " (App v" << selected->manifest.createdByAppVersion << ")\n\n";

        creation::assets::ProjectSession session;
        juce::String openError;
        if (creation::assets::ProjectSession::open(loadedSettings, selected->projectId, session, openError))
        {
            const auto entryPaths = session.listEntryPaths();
            text << "Asset Catalog (" << entryPaths.size() << " entries total):\n";
            int audioCount = 0, videoCount = 0, stateCount = 0;
            for (const auto& logicalPath : entryPaths)
            {
                if (logicalPath.endsWithIgnoreCase(".wav") || logicalPath.endsWithIgnoreCase(".mp3") || logicalPath.endsWithIgnoreCase(".flac"))
                    ++audioCount;
                else if (logicalPath.endsWithIgnoreCase(".mp4") || logicalPath.endsWithIgnoreCase(".mov"))
                    ++videoCount;
                else if (logicalPath.endsWithIgnoreCase(".xml") || logicalPath.endsWithIgnoreCase(".json"))
                    ++stateCount;
            }
            text << "  - Audio Files: " << audioCount << "\n";
            text << "  - Video Files: " << videoCount << "\n";
            text << "  - State / XML Files: " << stateCount << "\n";
        }
        else
        {
            text << "Asset Catalog Warning: " << openError << "\n";
        }

        auto cacheDir = creation::suite::getMaterializedFilesDirectory(loadedSettings, selected->manifest.projectId);
        if (cacheDir.isDirectory())
        {
            juce::Array<juce::File> cachedFiles;
            cacheDir.findChildFiles(cachedFiles, juce::File::findFiles, true);
            int64_t totalBytes = 0;
            for (const auto& f : cachedFiles)
                totalBytes += f.getSize();

            text << "\nMaterialized Cache: " << cachedFiles.size() << " files (" << juce::String(totalBytes / 1024.0 / 1024.0, 2) << " MB)\n";
        }
        else
        {
            text << "\nMaterialized Cache: Clean (No cached files on disk)\n";
        }

        detailsMetaText.setText(text);
    }

    void renameSelectedProject()
    {
        auto* selected = getSelectedProject();
        if (selected == nullptr)
            return;

        auto* prompt = new juce::AlertWindow("Rename Project",
                                             "Enter a new display name for project '" + selected->manifest.projectName + "':",
                                             juce::MessageBoxIconType::QuestionIcon);
        prompt->addTextEditor("newName", selected->manifest.projectName);
        prompt->addButton("Rename", 1);
        prompt->addButton("Cancel", 0);

        auto projectIdToRename = selected->projectId;
        auto options = juce::Component::SafePointer<SuiteProjectBrowserPanel>(this);
        prompt->enterModalState(true, juce::ModalCallbackFunction::create([options, prompt, projectIdToRename](int result) mutable
        {
            std::unique_ptr<juce::AlertWindow> dialog(prompt);
            if (result != 1 || options == nullptr)
                return;

            auto newName = dialog->getTextEditorContents("newName").trim();
            if (newName.isEmpty())
                return;

            creation::assets::ProjectSession session;
            juce::String err;
            if (creation::assets::ProjectSession::open(options->loadedSettings, projectIdToRename, session, err))
            {
                session.getManifest().projectName = newName;
                session.getManifest().modifiedAt = juce::Time::getCurrentTime();
                if (session.commit(err))
                    options->refresh();
            }
        }), true);
    }

    void beginCreateProject()
    {
        auto* prompt = new juce::AlertWindow("New Project",
                                             "Enter a name for the new project:",
                                             juce::MessageBoxIconType::QuestionIcon);
        prompt->addTextEditor("projectName", "New Project");
        prompt->addButton("Create Project", 1);
        prompt->addButton("Cancel", 0);

        auto domainForNewProject = currentDomain;
        auto options = juce::Component::SafePointer<SuiteProjectBrowserPanel>(this);
        prompt->enterModalState(true, juce::ModalCallbackFunction::create([options, prompt, domainForNewProject](int result) mutable
        {
            std::unique_ptr<juce::AlertWindow> dialog(prompt);
            if (result != 1 || options == nullptr)
                return;

            auto projectName = dialog->getTextEditorContents("projectName").trim();
            if (projectName.isEmpty())
                return;

            creation::assets::ProjectSession newSession;
            juce::String err;
            if (! creation::assets::ProjectContainerService::createProject(options->loadedSettings, domainForNewProject,
                    projectName, "1.0.0", "1.0.0", newSession, err))
            {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Create Project Error", err);
                return;
            }

            options->refresh();
            if (options->onProjectOpenRequested)
                options->onProjectOpenRequested(newSession.getProjectId());
        }), true);
    }

    void cloneSelectedProject()
    {
        auto* selected = getSelectedProject();
        if (selected == nullptr)
            return;

        auto defaultCloneName = selected->manifest.projectName + " (Copy)";
        auto* prompt = new juce::AlertWindow("Clone Project",
                                             "Enter a name for the cloned project container:",
                                             juce::MessageBoxIconType::QuestionIcon);
        prompt->addTextEditor("cloneName", defaultCloneName);
        prompt->addButton("Clone Project", 1);
        prompt->addButton("Cancel", 0);

        auto sourceProjectId = selected->projectId;
        auto options = juce::Component::SafePointer<SuiteProjectBrowserPanel>(this);
        prompt->enterModalState(true, juce::ModalCallbackFunction::create([options, prompt, sourceProjectId](int result) mutable
        {
            std::unique_ptr<juce::AlertWindow> dialog(prompt);
            if (result != 1 || options == nullptr)
                return;

            auto cloneName = dialog->getTextEditorContents("cloneName").trim();
            if (cloneName.isEmpty())
                return;

            // The VFS service does the actual folder-tree copy and re-mints the clone's
            // projectId server-side -- clients never touch project files directly.
            juce::String newProjectId, err;
            if (! creation::assets::ProjectContainerService::cloneProject(options->loadedSettings, sourceProjectId, cloneName, newProjectId, err))
            {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Clone Error", err);
                return;
            }

            options->refresh();
        }), true);
    }

    void purgeSelectedProjectCache()
    {
        auto* selected = getSelectedProject();
        if (selected == nullptr)
            return;

        auto cacheDir = creation::suite::getMaterializedFilesDirectory(loadedSettings, selected->manifest.projectId);
        if (cacheDir.isDirectory())
            cacheDir.deleteRecursively();

        updateSelectedProjectDetails();
    }

    void confirmAndDeleteSelectedProject()
    {
        auto* selected = getSelectedProject();
        if (selected == nullptr)
            return;

        auto projectIdToDelete = selected->manifest.projectId;
        auto projectName = selected->manifest.projectName;

        auto* confirmWindow = new juce::AlertWindow("DELETE PROJECT",
                                                     "Are you sure you want to PERMANENTLY delete project '" + projectName + "'?\n\n"
                                                     "This will remove the project folder from the suite VFS.\n"
                                                     "Type DELETE below to confirm:",
                                                     juce::MessageBoxIconType::WarningIcon);
        confirmWindow->addTextEditor("confirmText", "");
        confirmWindow->addButton("DELETE PROJECT PERMANENTLY", 1);
        confirmWindow->addButton("Cancel", 0);

        auto options = juce::Component::SafePointer<SuiteProjectBrowserPanel>(this);
        confirmWindow->enterModalState(true, juce::ModalCallbackFunction::create([options, confirmWindow, projectIdToDelete, projectName](int result) mutable
        {
            std::unique_ptr<juce::AlertWindow> dialog(confirmWindow);
            if (result != 1 || options == nullptr)
                return;

            auto typed = dialog->getTextEditorContents("confirmText").trim();
            if (typed != "DELETE")
            {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Delete Cancelled", "Deletion cancelled: You must type DELETE exactly to confirm deletion.");
                return;
            }

            juce::String deleteError;
            creation::assets::ProjectContainerService::deleteProject(options->loadedSettings, projectIdToDelete, deleteError);

            auto cacheDir = creation::suite::getMaterializedFilesDirectory(options->loadedSettings, projectIdToDelete);
            if (cacheDir.isDirectory())
                cacheDir.deleteRecursively();

            options->refresh();
        }), true);
    }

    creation::suite::SuiteSettings loadedSettings;
    creation::assets::SuiteAppDomain currentDomain;
    juce::Array<creation::assets::ProjectContainerService::ProjectSummary> allProjects;
    juce::Array<creation::assets::ProjectContainerService::ProjectSummary> filteredProjects;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::TextEditor searchEditor;
    juce::ComboBox domainCombo;
    juce::TextButton refreshButton;
    juce::TextButton newProjectButton;

    juce::ListBox listBox;

    juce::Label detailsHeaderLabel;
    juce::Label detailsNameLabel;
    juce::TextEditor detailsMetaText;

    juce::TextButton loadButton;
    juce::TextButton renameButton;
    juce::TextButton duplicateButton;
    juce::TextButton purgeCacheButton;
    juce::TextButton deleteButton;
};

}

namespace creation::ui
{
SuiteShellController::~SuiteShellController()
{
    suiteSettingsPanel = nullptr;
}

void SuiteShellController::configure(const Config& configToUse,
                                     std::function<void(const juce::String&)> statusSinkToUse)
{
    config = configToUse;
    statusSink = std::move(statusSinkToUse);

    juce::String suiteError;
    suiteSettings = suiteSettingsStore.load(suiteError);
    juce::String aiError;
    suiteAiSettings = suiteAiSettingsStore.load(aiError);

    authSession = std::make_unique<SuiteDesktopAuthSession>(suiteAuthAppSlug(config.appDomain));
    authSession->onStatusChanged = [this](const juce::String& text)
    {
        setStatus(text);
    };
    authSession->onError = [this](const juce::String& text)
    {
        setStatus(text);
    };
    authSession->onBusyChanged = [this](bool shouldBeBusy)
    {
        if (headerBar != nullptr)
            headerBar->signInButton.setEnabled(! shouldBeBusy);
    };
    authSession->onAuthenticated = [this](const SuiteDesktopAuthSession::SessionData& session)
    {
        if (headerBar != nullptr)
            headerBar->setProfile(makeProfileData(session));
        setStatus("Signed in to LagDaemon.");
    };
    authSession->onSessionCleared = [this]
    {
        if (headerBar != nullptr)
            headerBar->clearProfile();
        setStatus("Signed out.");
    };
}

void SuiteShellController::attach(CreationSuiteHeaderBar& headerBarToUse,
                                  const Config& configToUse,
                                  std::function<void(const juce::String&)> statusSinkToUse)
{
    headerBar = &headerBarToUse;
    configure(configToUse, std::move(statusSinkToUse));

    headerBar->onSuiteRequested = [this] { showSuiteSettingsWindow(); };
    headerBar->onProjectMenuRequested = [this] { showProjectBrowserWindow(); };
    headerBar->onAssetManagerRequested = [this] { showAssetManagerWindow(); };
    headerBar->onSignInRequested = [this] { beginSuiteSignIn(); };
    headerBar->onOpenProfilePageRequested = [this] { openLagDaemonProfile(); };
    headerBar->onLogoutRequested = [this] { logoutProfile(); };
    restoreProfileFromSession();
}

void SuiteShellController::showSuiteSettings()
{
    showSuiteSettingsWindow();
}

void SuiteShellController::showProjectBrowser()
{
    showProjectBrowserWindow();
}

bool SuiteShellController::openProject(const juce::String& projectId)
{
    juce::String errorMessage;
    if (! creation::assets::ProjectContainerService::openProject(
            suiteSettings, projectId, activeProjectSession, errorMessage))
    {
        setStatus("Could not open Suite project: " + errorMessage);
        return false;
    }

    if (headerBar != nullptr)
        headerBar->setProjectLabel("Project: " + activeProjectSession.getManifest().projectName);

    if (onProjectOpenRequested)
        onProjectOpenRequested(projectId);

    return true;
}

void SuiteShellController::showAssetManager()
{
    showAssetManagerWindow();
}

void SuiteShellController::showSuiteEula()
{
    showEulaWindow();
}

void SuiteShellController::openSuiteSignIn()
{
    beginSuiteSignIn();
}

void SuiteShellController::openSuiteProfile()
{
    openLagDaemonProfile();
}

void SuiteShellController::clearShellProfile()
{
    logoutProfile();
}

void SuiteShellController::showSuiteSettingsWindow()
{
    if (suiteSettingsWindow != nullptr)
    {
        suiteSettingsWindow->toFront(true);
        return;
    }

    auto panel = std::make_unique<SuiteSettingsPanel>();
    panel->setSettings(suiteSettings);
    panel->setAiSettings(suiteAiSettings);
    panel->onBrowseRequested = [this](const juce::String& fieldId) { chooseSuiteDirectory(fieldId); };
    panel->onApplyRequested = [this](const creation::suite::SuiteSettings& settings) { applySuiteSettings(settings); };
    panel->onApplyAiSettingsRequested = [this](const creation::services::SuiteAiSettings& settings) { applySuiteAiSettings(settings); };
    panel->onReadEulaRequested = [this] { showEulaWindow(); };

    auto* panelRaw = panel.get();
    auto window = std::make_unique<ManagedDocumentWindow>("Creation Suite Control",
                                                          config.backgroundColour,
                                                          juce::DocumentWindow::allButtons,
                                                          [this] { closeSuiteSettingsWindow(); });
    window->setUsingNativeTitleBar(true);
    window->setResizable(true, true);
    window->setContentOwned(panel.release(), true);
    window->centreWithSize(940, 560);
    window->setVisible(true);
    suiteSettingsPanel = panelRaw;
    suiteSettingsWindow = std::move(window);
    setStatus("Opened suite settings.");
}

void SuiteShellController::closeSuiteSettingsWindow()
{
    suiteSettingsPanel = nullptr;
    suiteSettingsWindow.reset();
}

void SuiteShellController::showProjectBrowserWindow()
{
    if (projectBrowserWindow != nullptr)
    {
        projectBrowserWindow->toFront(true);
        return;
    }

    auto panel = std::make_unique<SuiteProjectBrowserPanel>(config.appDomain);
    panel->onProjectOpenRequested = [this](const juce::String& projectId)
    {
        if (! openProject(projectId))
            return;

        // The project panel owns the button callback currently on the stack.
        // Defer destroying its window until that callback has returned.
        juce::MessageManager::callAsync([this] { closeProjectBrowserWindow(); });
    };
    auto window = std::make_unique<ManagedDocumentWindow>("Creation Suite Project Manager",
                                                          config.backgroundColour,
                                                          juce::DocumentWindow::allButtons,
                                                          [this] { closeProjectBrowserWindow(); });
    window->setUsingNativeTitleBar(true);
    window->setResizable(true, true);
    window->setContentOwned(panel.release(), true);
    window->centreWithSize(980, 620);
    window->setVisible(true);
    projectBrowserWindow = std::move(window);
    setStatus("Opened suite project browser.");
}

void SuiteShellController::closeProjectBrowserWindow()
{
    projectBrowserWindow.reset();
}

void SuiteShellController::showAssetManagerWindow()
{
    if (assetManagerWindow != nullptr)
    {
        assetManagerWindow->toFront(true);
        return;
    }

    auto capability = config.assetManagerCapability;
    if (capability.hostAppDisplayName.isEmpty())
        capability.hostAppDisplayName = config.appDisplayName;

    auto panel = std::make_unique<SuiteAssetManagerPanel>(capability);
    auto window = std::make_unique<ManagedDocumentWindow>("Creation Suite Asset Manager",
                                                          config.backgroundColour,
                                                          juce::DocumentWindow::allButtons,
                                                          [this] { closeAssetManagerWindow(); });
    window->setUsingNativeTitleBar(true);
    window->setResizable(true, true);
    window->setContentOwned(panel.release(), true);
    window->centreWithSize(980, 620);
    window->setVisible(true);
    assetManagerWindow = std::move(window);
    setStatus("Opened suite asset manager.");
}

void SuiteShellController::closeAssetManagerWindow()
{
    assetManagerWindow.reset();
}

void SuiteShellController::showEulaWindow()
{
    if (eulaWindow != nullptr)
    {
        eulaWindow->toFront(true);
        return;
    }

    auto window = std::make_unique<ManagedDocumentWindow>("Creation Suite EULA",
                                                          config.backgroundColour,
                                                          juce::DocumentWindow::allButtons,
                                                          [this] { closeEulaWindow(); });
    window->setUsingNativeTitleBar(true);
    window->setResizable(true, true);
    window->setContentOwned(new SuiteEulaPanel(), true);
    window->centreWithSize(880, 640);
    window->setVisible(true);
    eulaWindow = std::move(window);
    setStatus("Opened suite EULA.");
}

void SuiteShellController::closeEulaWindow()
{
    eulaWindow.reset();
}

void SuiteShellController::chooseSuiteDirectory(const juce::String& fieldId)
{
    juce::String currentPath = suiteSettings.suiteVfsRoot;

    suiteDirectoryChooser = std::make_unique<juce::FileChooser>("Choose a folder for the Creation Suite",
                                                                currentPath.isNotEmpty()
                                                                    ? juce::File(currentPath)
                                                                    : getDefaultSuiteStorageBrowseRoot(),
                                                                "*",
                                                                true);
    auto chooser = suiteDirectoryChooser.get();
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                         [this, chooser, fieldId](const juce::FileChooser& result)
                         {
                             auto selected = result.getResult();
                             if (chooser == suiteDirectoryChooser.get())
                                 suiteDirectoryChooser.reset();

                             if (selected == juce::File())
                                 return;

                             auto selectedPath = selected.getFullPathName();
                             if (fieldId == "suite_vfs_root")
                                 suiteSettings.suiteVfsRoot = selectedPath;

                             if (suiteSettingsPanel != nullptr)
                                 suiteSettingsPanel->setSettings(suiteSettings);
                         });
}

void SuiteShellController::applySuiteSettings(const creation::suite::SuiteSettings& settings)
{
    juce::String errorMessage;
    if (! suiteSettingsStore.save(settings, errorMessage))
    {
        if (suiteSettingsPanel != nullptr)
            suiteSettingsPanel->setStatusText(errorMessage);
        setStatus(errorMessage);
        return;
    }

    suiteSettings = settings;
    if (suiteSettingsPanel != nullptr)
        suiteSettingsPanel->setStatusText("Saved suite-wide settings.");
    if (projectBrowserWindow != nullptr)
    {
        closeProjectBrowserWindow();
        showProjectBrowserWindow();
    }
    setStatus("Saved suite-wide settings.");
}

void SuiteShellController::applySuiteAiSettings(const creation::services::SuiteAiSettings& settings)
{
    juce::String errorMessage;
    if (! suiteAiSettingsStore.save(settings, errorMessage))
    {
        if (suiteSettingsPanel != nullptr)
            suiteSettingsPanel->setStatusText(errorMessage);
        setStatus(errorMessage);
        return;
    }

    suiteAiSettings = settings;
    if (suiteSettingsPanel != nullptr)
        suiteSettingsPanel->setStatusText("Saved suite-wide AI routing settings.");
    setStatus("Saved suite-wide AI routing settings.");
}

void SuiteShellController::beginSuiteSignIn()
{
    if (authSession == nullptr)
    {
        openLagDaemonHome();
        return;
    }

    authSession->beginLogin();
}

void SuiteShellController::openLagDaemonHome()
{
    if (juce::URL("https://lagdaemon.com").launchInDefaultBrowser())
        setStatus("Opened LagDaemon sign-in in your browser.");
    else
        setStatus("Could not open LagDaemon in your browser.");
}

void SuiteShellController::openLagDaemonProfile()
{
    if (juce::URL("https://lagdaemon.com/account").launchInDefaultBrowser())
        setStatus("Opened your LagDaemon profile in your browser.");
    else
        setStatus("Could not open the LagDaemon profile page.");
}

void SuiteShellController::logoutProfile()
{
    if (authSession != nullptr)
        authSession->clearSession();
    else if (headerBar != nullptr)
        headerBar->clearProfile();
}

void SuiteShellController::restoreProfileFromSession()
{
    if (authSession == nullptr || headerBar == nullptr)
        return;

    if (authSession->loadFromDisk() && authSession->hasValidSession())
    {
        headerBar->setProfile(makeProfileData(authSession->getSession()));
        setStatus("Restored your LagDaemon session.");
    }
    else
    {
        headerBar->clearProfile();
    }
}

CreationSuiteHeaderBar::ProfileData SuiteShellController::makeProfileData(const SuiteDesktopAuthSession::SessionData& session) const
{
    CreationSuiteHeaderBar::ProfileData profile;
    profile.displayName = session.user.displayName.isNotEmpty() ? session.user.displayName : session.user.email;
    profile.detailText = profileDetailText(session);
    return profile;
}

void SuiteShellController::setStatus(const juce::String& text) const
{
    if (statusSink)
        statusSink(text);
    else if (headerBar != nullptr)
        headerBar->setStatusText(text);
}
}
