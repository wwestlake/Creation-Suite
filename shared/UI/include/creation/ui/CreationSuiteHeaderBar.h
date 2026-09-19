#pragma once

#include <creation/ui/CreationSuiteLogos.h>
#include <juce_gui_extra/juce_gui_extra.h>

// A text button that folds down to a drawn icon when the header runs out of room (never disappearing), with
// its full name (and, for the project button, the current project) shown as a hover tooltip.
class HeaderIconButton final : public juce::TextButton
{
public:
    enum class Icon { none, project, audio, assets, pods, tour, about };

    explicit HeaderIconButton(const juce::String& label) : juce::TextButton(label) {}

    void setIcon(Icon newIcon) { icon = newIcon; }
    void setCompact(bool shouldBeCompact)
    {
        if (compact == shouldBeCompact)
            return;
        compact = shouldBeCompact;
        repaint();
    }
    bool isCompact() const noexcept { return compact; }
    // What the button says when there is room for words; used as the hover text once it is an icon.
    void setFullLabel(const juce::String& text) { fullLabel = text; }

    juce::String getTooltip() override
    {
        const auto hint = juce::TextButton::getTooltip();
        if (! compact || fullLabel.isEmpty())
            return hint;
        return hint.isEmpty() ? fullLabel : fullLabel + " - " + hint;
    }

    void paintButton(juce::Graphics& g, bool isMouseOver, bool isButtonDown) override;

private:
    Icon icon = Icon::none;
    bool compact = false;
    juce::String fullLabel;
};

class CreationSuiteHeaderBar final : public juce::Component
{
public:
    enum class TransportButtonSlot
    {
        rewind,
        fastForward,
        stop,
        playPause,
        loop,
        click,
        record
    };

    enum class MetronomeMode
    {
        off,
        playOrRecord,
        always
    };

    struct ProfileData
    {
        juce::String displayName;
        juce::String detailText;
        juce::Image badgeImage;
    };

    // How tall the header wants to be. Every app gives it this height, so the whole suite agrees. The controls keep
    // their own size; the height is room between the title row and the transport row.
    static constexpr int preferredHeight = 112;

    CreationSuiteHeaderBar();
    ~CreationSuiteHeaderBar() override;

    std::function<void()> onPlay;
    std::function<void()> onPause;
    std::function<void()> onStop;
    std::function<void()> onRecord;
    std::function<void()> onRewind;
    std::function<void()> onFastForward;
    // Optional: fired instead of onRewind/onFastForward when the button is Ctrl-clicked. Apps
    // that don't set these keep today's plain-click-only behavior unchanged.
    std::function<void()> onRewindToStart;
    std::function<void()> onFastForwardToEnd;
    std::function<void(bool)> onLoopChanged;
    std::function<void(double)> onLoopDelayChanged;
    std::function<void(MetronomeMode)> onMetronomeModeChanged;
    std::function<void()> onSignInRequested;
    std::function<void()> onOpenProfilePageRequested;
    std::function<void()> onLogoutRequested;
    std::function<void()> onProjectMenuRequested;
    std::function<void()> onTourRequested;
    std::function<void()> onAudioRequested;
    std::function<void()> onSuiteRequested;
    std::function<void()> onAssetManagerRequested;
    std::function<void()> onPodsRequested;
    std::function<void(const juce::String& targetId, const juce::String& displayLabel)> onLearnMidiRequested;

    void setAppTitle(const juce::String& title);
    void setAppLogo(creation::ui::SuiteLogoId logoId);
    void setSelectedLogoId(creation::ui::SuiteLogoId logoId);
    void setLogoImage(const juce::Image& image);
    void setProfile(const ProfileData& profile);
    void clearProfile();
    void setProjectLabel(const juce::String& label);
    juce::Rectangle<int> getProjectButtonScreenBounds() const;
    void setStatusText(const juce::String& text);
    // The status label is small and a message in it is cut off and soon replaced, which is no way to show an
    // error. When this is set, a status message that reads as an error is handed to it instead of the label,
    // so the app can show it somewhere with room to read (a dialog).
    std::function<void(const juce::String& message)> onErrorStatus;
    static bool statusTextIsError(const juce::String& text);
    // Where non-error status messages go when the app supplies a better place than the label (a toast).
    std::function<void(const juce::String& message)> onInfoStatus;
    // The small status label can be taken off the screen entirely; messages then reach the app only through
    // onErrorStatus / onInfoStatus.
    void setStatusLabelVisible(bool shouldBeVisible) { statusLabel.setVisible(shouldBeVisible); resized(); }
    void setMidiStatusText(const juce::String& text);
    void setPlaybackVisualState(bool playing, bool recording);
    void setMetronomeMode(MetronomeMode mode);
    MetronomeMode getMetronomeMode() const noexcept { return metronomeMode; }
    void setScrubModeEnabled(bool enabled);
    void setTransportControlsVisible(bool shouldBeVisible);
    void setTransportButtonVisible(TransportButtonSlot slot, bool shouldBeVisible);
    void setTransportButtonEnabled(TransportButtonSlot slot, bool shouldBeEnabled);
    // Audio applications may present Play/Pause as one changing button. A
    // simulation editor benefits from distinct controls: Play starts or
    // resumes, Pause freezes, and Stop discards the runtime state.
    void setSeparatePauseButtonVisible(bool shouldBeVisible);

    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent& event) override;

private:
    struct TransportButtonConfig
    {
        bool visible = true;
        bool enabled = true;
    };

    static juce::String makeInitials(const juce::String& displayName, const juce::String& detailText);
    TransportButtonConfig& getTransportButtonConfig(TransportButtonSlot slot);
    const TransportButtonConfig& getTransportButtonConfig(TransportButtonSlot slot) const;
    void refreshTransportButtonPresentation();
    void advanceMetronomeMode();
    void refreshMetronomeButton();

public:
    juce::Label titleLabel;
    juce::Image logoImage;
    juce::Label midiStatusLabel;
    juce::TextButton playButton { "Play" };
    juce::TextButton pauseButton { "Pause" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton recordButton { "Record" };
    juce::ToggleButton loopButton { "Loop" };
    juce::Slider loopDelaySlider;
    juce::ToggleButton clickButton { "Click" };
    juce::TextButton rewindButton { "Rew" };
    juce::TextButton fastForwardButton { "Fwd" };
    juce::TextButton signInButton { "Sign In" };
    HeaderIconButton projectButton { "Project" };
    HeaderIconButton audioButton { "Audio" };
    HeaderIconButton assetsButton { "Assets" };
    HeaderIconButton podsButton { "Pods" };
    juce::TextButton suiteButton { juce::String(juce::CharPointer_UTF8("\xe2\x9a\x99")) };
    HeaderIconButton tourButton { "Tour" };
    // Opens the shared Help/About box (creation::ui::SuiteJUCEApplication::
    // showAboutBox) -- wired entirely inside CreationSuiteHeaderBar.cpp via
    // juce::JUCEApplication::getInstance(), never via an app-supplied
    // callback, so any app that already constructs this header bar and
    // derives its JUCEApplication subclass from SuiteJUCEApplication gets a
    // working About entry with zero additional wiring.
    HeaderIconButton aboutButton { "?" };
    juce::Label profileNameLabel;
    juce::Label profileDetailLabel;
    juce::Image profileBadgeImage;
    juce::Label statusLabel;
    juce::Label scrubModeLabel;
    juce::Rectangle<int> transportControlBounds;

private:
    juce::String profileInitials;
    juce::String projectText;
    juce::Rectangle<int> profileChipBounds;
    juce::Rectangle<int> logoRailBounds;
    creation::ui::SuiteLogoId selectedLogoId = creation::ui::SuiteLogoId::suite;
    bool profileVisible = false;
    bool scrubModeEnabled = false;
    bool playbackIsPlaying = false;
    bool playbackIsRecording = false;
    MetronomeMode metronomeMode = MetronomeMode::off;
    bool metronomeAudible = false;
    bool transportControlsVisible = true;
    bool separatePauseButtonVisible = false;
    TransportButtonConfig rewindButtonConfig;
    TransportButtonConfig fastForwardButtonConfig;
    TransportButtonConfig stopButtonConfig;
    TransportButtonConfig playPauseButtonConfig;
    TransportButtonConfig loopButtonConfig;
    TransportButtonConfig clickButtonConfig;
    TransportButtonConfig recordButtonConfig;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationSuiteHeaderBar)
};
