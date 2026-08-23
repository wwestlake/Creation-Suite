#pragma once

#include <creation/ui/CreationSuiteLogos.h>
#include <juce_gui_extra/juce_gui_extra.h>

namespace creation::ui
{
// Every Creation Suite app's juce::JUCEApplication subclass should inherit
// from this instead of juce::JUCEApplication directly. initialise()/
// shutdown() are implemented HERE, not virtual -- the whole point is that
// showing the splash on startup (and making the same panel available later
// as a Help/About box) is structural, not something each app's Main.cpp has
// to remember to wire up itself. Before this class existed, only one app in
// the suite (CreationStation) had a splash at all, hand-rolled entirely in
// its own Main.cpp -- nothing forced any other app to copy it, which is
// exactly the same class of gap that once left an app with no window icon
// either. A derived class only needs to implement createMainWindow().
//
// Minimum-visible-time is enforced with a juce::Timer, never a blocking
// sleep() on the message thread -- the message loop keeps running the whole
// time so the splash actually paints and the app stays responsive.
class SuiteJUCEApplication : public juce::JUCEApplication, private juce::Timer
{
public:
    explicit SuiteJUCEApplication(SuiteLogoId logoId, int minimumSplashVisibleMs = 5000);
    ~SuiteJUCEApplication() override;

    // Sealed. See the class comment for why -- a derived app implements
    // createMainWindow() instead of overriding these.
    void initialise(const juce::String& commandLine) final;
    void shutdown() final;

    // Opens the same shared panel used for the startup splash, in
    // Mode::about instead: a user-dismissed dialog (Close button) showing
    // this app's name/version (getApplicationName()/getApplicationVersion(),
    // which every juce::JUCEApplication subclass already implements) and
    // the suite's shared copyright line (getSuiteCopyrightText()).
    // CreationSuiteHeaderBar's About button calls this via
    // juce::JUCEApplication::getInstance() + a dynamic_cast -- zero
    // per-app wiring needed for apps that already use the header bar.
    void showAboutBox();

protected:
    // Build and show the app's real main window. Called once, shortly
    // after the splash has already had a chance to paint (via callAsync
    // from initialise(), never synchronously from inside it).
    virtual std::unique_ptr<juce::DocumentWindow> createMainWindow() = 0;

    // Non-owning access to the main window this class is holding, for a
    // subclass that needs to reach it outside createMainWindow() itself
    // (e.g. a systemRequestedQuit() override that needs to call a method on
    // its own MainWindow subclass, like CreationStation's confirm-before-
    // quit flow). Prefer this over a subclass keeping its own raw pointer
    // to the same window -- this one automatically reads nullptr once
    // shutdown() has cleared it; a subclass's own copy would not, since
    // shutdown() is sealed here and can't notify a subclass to null its copy.
    juce::DocumentWindow* getMainWindow() const noexcept { return mainWindow_.get(); }

private:
    void timerCallback() override; // closes the splash once the minimum-visible time is up
    void finishStartup();          // callAsync target: builds the main window, starts/skips the close timer
    void closeSplash();

    class SplashWindow;
    class AboutWindow;

    SuiteLogoId logoId_;
    int minimumSplashVisibleMs_;
    juce::int64 splashShownAtMs_ = 0;
    std::unique_ptr<SplashWindow> splashWindow_;
    std::unique_ptr<juce::DocumentWindow> mainWindow_;
    std::unique_ptr<AboutWindow> aboutWindow_;
};
}
