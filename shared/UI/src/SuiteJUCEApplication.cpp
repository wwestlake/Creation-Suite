#include <creation/ui/SuiteJUCEApplication.h>

#include <creation/ui/SuiteCommonSpacePanel.h>

#if JUCE_WINDOWS
 #include <windows.h>
#endif

namespace
{
// Pumps the OS message queue for a short window so a just-shown window
// actually paints before the caller goes on to do (potentially blocking-ish)
// startup work -- ported verbatim from CreationStation's own Main.cpp,
// the one app in the suite that already had a correct, working splash
// sequence before this class existed.
void pumpStartupPaintMessages(int milliseconds)
{
   #if JUCE_WINDOWS
    const auto endTime = juce::Time::getMillisecondCounter() + (juce::uint32) juce::jmax(1, milliseconds);
    MSG message;

    do
    {
        while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE) != 0)
        {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        juce::Thread::sleep(1);
    }
    while (juce::Time::getMillisecondCounter() < endTime);
   #else
    juce::Thread::sleep(milliseconds);
   #endif
}
}

namespace creation::ui
{
// Small DocumentWindow wrapping one SuiteCommonSpacePanel in Mode::splash --
// no primary/secondary action (nothing to click; it closes itself on the
// minimum-visible timer, see SuiteJUCEApplication::timerCallback).
class SuiteJUCEApplication::SplashWindow final : public juce::DocumentWindow
{
public:
    SplashWindow(const juce::String& appDisplayName, SuiteLogoId logoId, const juce::String& versionText)
        : juce::DocumentWindow(appDisplayName, juce::Colour(0xff0b0f14), 0)
    {
        setUsingNativeTitleBar(false);
        setTitleBarHeight(0);
        setDropShadowEnabled(true);
        setResizable(false, false);
        setAlwaysOnTop(true);
        setOpaque(true);
        setBackgroundColour(juce::Colour(0xff0b0f14));

        panel.setMode(SuiteCommonSpacePanel::Mode::splash);
        panel.setSelectedLogoId(logoId);
        panel.setStatusText("Starting " + appDisplayName + "...");
        panel.setProgress(0.02f);
        panel.setVersionText(versionText.isNotEmpty() ? "v" + versionText : juce::String());
        panel.setCopyrightText(getSuiteCopyrightText());
        setContentNonOwned(&panel, true);
        centreWithSize(1040, 620);
    }

private:
    SuiteCommonSpacePanel panel;
};

// Same shape as SplashWindow, in Mode::about instead: a user-dismissed
// dialog with a Close button (via setPrimaryAction), never auto-closes.
class SuiteJUCEApplication::AboutWindow final : public juce::DocumentWindow
{
public:
    AboutWindow(const juce::String& appDisplayName, const juce::String& versionText, SuiteLogoId logoId,
               std::function<void()> onClose)
        : juce::DocumentWindow(appDisplayName + " - About", juce::Colour(0xff0b0f14), 0)
    {
        setUsingNativeTitleBar(false);
        setTitleBarHeight(0);
        setDropShadowEnabled(true);
        setResizable(false, false);
        setAlwaysOnTop(true);
        setOpaque(true);
        setBackgroundColour(juce::Colour(0xff0b0f14));

        panel.setMode(SuiteCommonSpacePanel::Mode::about);
        panel.setSelectedLogoId(logoId);
        panel.setStatusText(appDisplayName);
        panel.setVersionText(versionText.isNotEmpty() ? "v" + versionText : juce::String());
        panel.setCopyrightText(getSuiteCopyrightText());
        panel.setPrimaryAction("Close", std::move(onClose));
        setContentNonOwned(&panel, true);
        centreWithSize(1040, 620);
    }

private:
    SuiteCommonSpacePanel panel { SuiteCommonSpacePanel::Mode::about };
};

SuiteJUCEApplication::SuiteJUCEApplication(SuiteLogoId logoId, int minimumSplashVisibleMs)
    : logoId_(logoId), minimumSplashVisibleMs_(minimumSplashVisibleMs)
{
}

SuiteJUCEApplication::~SuiteJUCEApplication() = default;

void SuiteJUCEApplication::initialise(const juce::String&)
{
    splashWindow_ = std::make_unique<SplashWindow>(getApplicationName(), logoId_, getApplicationVersion());
    splashWindow_->setVisible(true);
    splashWindow_->toFront(true);
    pumpStartupPaintMessages(40);
    splashShownAtMs_ = static_cast<juce::int64>(juce::Time::getMillisecondCounter());

    juce::MessageManager::callAsync([this] { finishStartup(); });
}

void SuiteJUCEApplication::finishStartup()
{
    // Each app's own MainWindow constructor already calls setVisible(true)
    // itself (unchanged behavior from before this class existed), so
    // createMainWindow() returns an already-visible window.
    mainWindow_ = createMainWindow();

    const auto elapsedMs = static_cast<int>(juce::Time::getMillisecondCounter()
                                            - static_cast<juce::uint32>(splashShownAtMs_));
    const auto remainingMs = minimumSplashVisibleMs_ - elapsedMs;
    if (remainingMs <= 0)
        juce::MessageManager::callAsync([this] { closeSplash(); });
    else
        startTimer(remainingMs);
}

void SuiteJUCEApplication::timerCallback()
{
    stopTimer();
    closeSplash();
}

void SuiteJUCEApplication::closeSplash()
{
    splashWindow_ = nullptr;
}

void SuiteJUCEApplication::shutdown()
{
    stopTimer();
    aboutWindow_ = nullptr;
    mainWindow_ = nullptr;
    splashWindow_ = nullptr;
}

void SuiteJUCEApplication::showAboutBox()
{
    if (aboutWindow_ != nullptr)
    {
        aboutWindow_->toFront(true);
        return;
    }

    // Deferred via callAsync, not called synchronously -- the Close button
    // lives inside the very window this callback destroys, and deleting a
    // component synchronously from inside its own onClick handler (still on
    // the stack, still dispatching that same click event) is exactly the
    // kind of self-destruction-during-event-handling hazard JUCE code
    // avoids by posting the deletion instead. Same discipline closeSplash's
    // remainingMs<=0 path already follows.
    aboutWindow_ = std::make_unique<AboutWindow>(getApplicationName(), getApplicationVersion(), logoId_,
                                                 [this] { juce::MessageManager::callAsync([this] { aboutWindow_ = nullptr; }); });
    aboutWindow_->setVisible(true);
    aboutWindow_->toFront(true);
}
}
