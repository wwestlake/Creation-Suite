#pragma once

#include <creation/ui/CreationSuiteLogos.h>
#include <juce_gui_extra/juce_gui_extra.h>

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

    CreationSuiteHeaderBar();
    ~CreationSuiteHeaderBar() override;

    std::function<void()> onPlay;
    std::function<void()> onPause;
    std::function<void()> onStop;
    std::function<void()> onRecord;
    std::function<void()> onRewind;
    std::function<void()> onFastForward;
    std::function<void(bool)> onLoopChanged;
    std::function<void(MetronomeMode)> onMetronomeModeChanged;
    std::function<void()> onSignInRequested;
    std::function<void()> onOpenProfilePageRequested;
    std::function<void()> onLogoutRequested;
    std::function<void()> onProjectMenuRequested;
    std::function<void()> onTourRequested;
    std::function<void()> onAudioRequested;
    std::function<void()> onSuiteRequested;
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
    void setMidiStatusText(const juce::String& text);
    void setPlaybackVisualState(bool playing, bool recording);
    void setMetronomeMode(MetronomeMode mode);
    MetronomeMode getMetronomeMode() const noexcept { return metronomeMode; }
    void setScrubModeEnabled(bool enabled);
    void setTransportControlsVisible(bool shouldBeVisible);
    void setTransportButtonVisible(TransportButtonSlot slot, bool shouldBeVisible);
    void setTransportButtonEnabled(TransportButtonSlot slot, bool shouldBeEnabled);

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
    juce::ToggleButton clickButton { "Click" };
    juce::TextButton rewindButton { "Rew" };
    juce::TextButton fastForwardButton { "Fwd" };
    juce::TextButton signInButton { "Sign In" };
    juce::TextButton projectButton { "Project" };
    juce::TextButton audioButton { "Audio" };
    juce::TextButton suiteButton { juce::String(juce::CharPointer_UTF8("\xe2\x9a\x99")) };
    juce::TextButton tourButton { "Tour" };
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
    TransportButtonConfig rewindButtonConfig;
    TransportButtonConfig fastForwardButtonConfig;
    TransportButtonConfig stopButtonConfig;
    TransportButtonConfig playPauseButtonConfig;
    TransportButtonConfig loopButtonConfig;
    TransportButtonConfig clickButtonConfig;
    TransportButtonConfig recordButtonConfig;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationSuiteHeaderBar)
};
