#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <creation/assets/ProjectContainerService.h>
#include <creation/suite/SuiteSettings.h>
#include <creation/ui/CreationSuiteHeaderBar.h>
#include <creation/ui/SuiteAssetManagerCapability.h>
#include <creation/ui/SuiteDesktopAuthSession.h>
#include <creation/ui/SuiteSettingsPanel.h>

namespace creation::ui
{
class SuiteShellController final
{
public:
    struct Config
    {
        juce::String appDisplayName;
        creation::assets::SuiteAppDomain appDomain = creation::assets::SuiteAppDomain::unknown;
        juce::Colour backgroundColour { juce::Colour(0xff10161f) };
        SuiteAssetManagerCapability assetManagerCapability;
    };

    SuiteShellController() = default;
    ~SuiteShellController();

    void configure(const Config& config,
                   std::function<void(const juce::String&)> statusSink = {});
    void attach(CreationSuiteHeaderBar& headerBar,
                const Config& config,
                std::function<void(const juce::String&)> statusSink = {});
    void showSuiteSettings();
    void showProjectBrowser();
    void showAssetManager();
    void showSuiteEula();
    void openSuiteSignIn();
    void openSuiteProfile();
    void clearShellProfile();

    std::function<void(const juce::String& projectId)> onProjectOpenRequested;

private:
    void showSuiteSettingsWindow();
    void closeSuiteSettingsWindow();
    void showProjectBrowserWindow();
    void closeProjectBrowserWindow();
    void showAssetManagerWindow();
    void closeAssetManagerWindow();
    void showEulaWindow();
    void closeEulaWindow();
    void chooseSuiteDirectory(const juce::String& fieldId);
    void applySuiteSettings(const creation::suite::SuiteSettings& settings);
    void applySuiteAiSettings(const creation::services::SuiteAiSettings& settings);
    void beginSuiteSignIn();
    void openLagDaemonHome();
    void openLagDaemonProfile();
    void logoutProfile();
    void restoreProfileFromSession();
    CreationSuiteHeaderBar::ProfileData makeProfileData(const SuiteDesktopAuthSession::SessionData& session) const;
    void setStatus(const juce::String& text) const;

    CreationSuiteHeaderBar* headerBar = nullptr;
    Config config;
    std::function<void(const juce::String&)> statusSink;
    creation::suite::SuiteSettingsStore suiteSettingsStore;
    creation::services::SuiteAiSettingsStore suiteAiSettingsStore;
    creation::suite::SuiteSettings suiteSettings;
    creation::services::SuiteAiSettings suiteAiSettings;
    std::unique_ptr<juce::DocumentWindow> suiteSettingsWindow;
    std::unique_ptr<juce::DocumentWindow> projectBrowserWindow;
    std::unique_ptr<juce::DocumentWindow> assetManagerWindow;
    std::unique_ptr<juce::DocumentWindow> eulaWindow;
    juce::Component::SafePointer<SuiteSettingsPanel> suiteSettingsPanel;
    std::unique_ptr<juce::FileChooser> suiteDirectoryChooser;
    std::unique_ptr<SuiteDesktopAuthSession> authSession;
};
}
