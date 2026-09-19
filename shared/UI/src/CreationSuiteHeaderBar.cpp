#include <creation/ui/CreationSuiteHeaderBar.h>
#include <creation/ui/CreationSuiteLogos.h>
#include <creation/ui/SuiteJUCEApplication.h>
#include <creation/ui/TransportActionIds.h>

namespace
{
class TransportButtonLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isMouseOverButton,
                              bool isButtonDown) override
    {
        juce::ignoreUnused(backgroundColour);

        auto bounds = button.getLocalBounds().toFloat().reduced(1.5f);
        auto isToggle = button.getToggleState();
        auto accent = juce::Colour(0xff59dfff);
        auto fill = juce::Colour(0xff17222c);

        if (button.getName() == "recordButton")
            accent = juce::Colour(0xffff5f73);

        if (isToggle)
            fill = accent.withAlpha(0.25f).overlaidWith(juce::Colour(0xff13202b));
        else if (isButtonDown)
            fill = accent.withAlpha(0.20f).overlaidWith(fill);
        else if (isMouseOverButton)
            fill = accent.withAlpha(0.12f).overlaidWith(fill);

        g.setColour(accent.withAlpha(isToggle ? 0.35f : isMouseOverButton ? 0.35f : 0.14f));
        g.fillRoundedRectangle(bounds.expanded(2.0f), 13.0f);
        g.setColour(fill);
        g.fillRoundedRectangle(bounds, 11.0f);

        g.setColour(accent.withAlpha(isToggle ? 1.0f : 0.62f));
        g.drawRoundedRectangle(bounds, 11.0f, isToggle ? 2.0f : 1.3f);

        auto ring = bounds.reduced(7.0f, 5.0f);
        if (ring.getWidth() > 18.0f && ring.getHeight() > 18.0f)
        {
            auto diameter = juce::jmin(ring.getWidth(), ring.getHeight());
            auto circle = juce::Rectangle<float>(diameter, diameter).withCentre(ring.getCentre());
            g.setColour(accent.withAlpha(isToggle ? 0.96f : 0.36f));
            g.drawEllipse(circle, isToggle ? 2.4f : 2.0f);
        }
    }

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool isMouseOverButton,
                        bool isButtonDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(8.0f, 7.0f);

        g.setColour(button.getToggleState() ? juce::Colours::white
                                            : (isButtonDown ? juce::Colour(0xffeaf6ff)
                                                            : isMouseOverButton ? juce::Colour(0xffdcecff)
                                                                                : juce::Colour(0xffb8c4d5)));
        drawTransportIcon(g, bounds, button.getButtonText());
    }

    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool isMouseOverButton,
                          bool isButtonDown) override
    {
        drawButtonBackground(g,
                             button,
                             button.findColour(juce::TextButton::buttonColourId),
                             isMouseOverButton,
                             isButtonDown);

        auto bounds = button.getLocalBounds().toFloat().reduced(8.0f, 7.0f);
        g.setColour(button.getToggleState() ? juce::Colour(0xff5ce8ff) : juce::Colour(0xffb8c4d5));
        drawTransportIcon(g, bounds, button.getButtonText());
    }

private:
    static void drawTransportIcon(juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& iconName)
    {
        auto centre = bounds.getCentre();
        auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());

        if (iconName == "play")
        {
            juce::Path path;
            path.addTriangle(centre.x - size * 0.22f, centre.y - size * 0.32f,
                             centre.x - size * 0.22f, centre.y + size * 0.32f,
                             centre.x + size * 0.32f, centre.y);
            g.fillPath(path);
            return;
        }

        if (iconName == "pause")
        {
            auto barWidth = size * 0.16f;
            auto barHeight = size * 0.58f;
            auto leftBar = juce::Rectangle<float>(barWidth, barHeight).withCentre({ centre.x - size * 0.12f, centre.y });
            auto rightBar = juce::Rectangle<float>(barWidth, barHeight).withCentre({ centre.x + size * 0.12f, centre.y });
            g.fillRoundedRectangle(leftBar, 1.5f);
            g.fillRoundedRectangle(rightBar, 1.5f);
            return;
        }

        if (iconName == "stop")
        {
            auto square = juce::Rectangle<float>(size * 0.55f, size * 0.55f).withCentre(centre);
            g.fillRoundedRectangle(square, 2.0f);
            return;
        }

        if (iconName == "record")
        {
            auto circle = juce::Rectangle<float>(size * 0.48f, size * 0.48f).withCentre(centre);
            g.fillEllipse(circle);
            return;
        }

        if (iconName == "loop")
        {
            auto arc = bounds.reduced(size * 0.12f);
            g.drawEllipse(arc, 2.0f);
            juce::Path arrow;
            arrow.addTriangle(arc.getRight() - size * 0.02f, arc.getCentreY() - size * 0.20f,
                              arc.getRight() + size * 0.16f, arc.getCentreY() - size * 0.06f,
                              arc.getRight() - size * 0.02f, arc.getCentreY() + size * 0.08f);
            g.fillPath(arrow);
            return;
        }

        if (iconName == "click")
        {
            auto circle = bounds.reduced(size * 0.18f);
            g.drawEllipse(circle, 2.0f);
            auto point = circle.withSizeKeepingCentre(size * 0.16f, size * 0.16f);
            g.fillEllipse(point);
            return;
        }

        if (iconName == "prev" || iconName == "next")
        {
            const auto flip = iconName == "prev" ? -1.0f : 1.0f;
            juce::Path path;
            path.addTriangle(centre.x - flip * size * 0.04f, centre.y,
                             centre.x + flip * size * 0.20f, centre.y - size * 0.24f,
                             centre.x + flip * size * 0.20f, centre.y + size * 0.24f);
            path.addTriangle(centre.x - flip * size * 0.30f, centre.y,
                             centre.x - flip * size * 0.06f, centre.y - size * 0.24f,
                             centre.x - flip * size * 0.06f, centre.y + size * 0.24f);
            g.fillPath(path);
            return;
        }

        if (iconName == "gear" || iconName == "\xe2\x9a\x99")
        {
            const auto radius = size * 0.28f;
            juce::Path gearPath;
            const int numTeeth = 8;
            for (int i = 0; i < numTeeth; ++i)
            {
                float angle = i * juce::MathConstants<float>::twoPi / numTeeth;
                float outerR = radius * 1.25f;
                float innerR = radius * 0.85f;

                float a1 = angle - 0.18f;
                float a2 = angle - 0.09f;
                float a3 = angle + 0.09f;
                float a4 = angle + 0.18f;

                if (i == 0)
                    gearPath.startNewSubPath(centre.x + innerR * std::cos(a1), centre.y + innerR * std::sin(a1));
                else
                    gearPath.lineTo(centre.x + innerR * std::cos(a1), centre.y + innerR * std::sin(a1));

                gearPath.lineTo(centre.x + outerR * std::cos(a2), centre.y + outerR * std::sin(a2));
                gearPath.lineTo(centre.x + outerR * std::cos(a3), centre.y + outerR * std::sin(a3));
                gearPath.lineTo(centre.x + innerR * std::cos(a4), centre.y + innerR * std::sin(a4));
            }
            gearPath.closeSubPath();
            g.fillPath(gearPath);

            juce::Path holePath;
            holePath.addEllipse(centre.x - radius * 0.45f, centre.y - radius * 0.45f, radius * 0.90f, radius * 0.90f);
            g.setColour(juce::Colour(0xff17222c));
            g.fillPath(holePath);
            return;
        }
    }
};

void drawHeaderIcon(juce::Graphics& g, juce::Rectangle<float> area, HeaderIconButton::Icon icon)
{
    const auto size = juce::jmin(area.getWidth(), area.getHeight());
    const auto centre = area.getCentre();
    const auto box = juce::Rectangle<float>(size, size).withCentre(centre).reduced(size * 0.1f);
    const auto stroke = juce::jmax(1.5f, size * 0.09f);

    switch (icon)
    {
        case HeaderIconButton::Icon::project: // a folder
        {
            juce::Path folder;
            const auto x = box.getX(), y = box.getY() + box.getHeight() * 0.18f, w = box.getWidth(), h = box.getHeight() * 0.68f;
            folder.startNewSubPath(x, y + h * 0.18f);
            folder.lineTo(x, y + h);
            folder.lineTo(x + w, y + h);
            folder.lineTo(x + w, y + h * 0.18f);
            folder.lineTo(x + w * 0.45f, y + h * 0.18f);
            folder.lineTo(x + w * 0.35f, y - h * 0.08f);
            folder.lineTo(x + w * 0.05f, y - h * 0.08f);
            folder.closeSubPath();
            g.strokePath(folder, juce::PathStrokeType(stroke, juce::PathStrokeType::curved));
            break;
        }
        case HeaderIconButton::Icon::audio: // a speaker with a wave
        {
            juce::Path speaker;
            const auto x = box.getX(), cy = box.getCentreY(), w = box.getWidth(), h = box.getHeight();
            speaker.addRectangle(x, cy - h * 0.14f, w * 0.22f, h * 0.28f);
            speaker.startNewSubPath(x + w * 0.22f, cy - h * 0.14f);
            speaker.lineTo(x + w * 0.52f, cy - h * 0.36f);
            speaker.lineTo(x + w * 0.52f, cy + h * 0.36f);
            speaker.lineTo(x + w * 0.22f, cy + h * 0.14f);
            speaker.closeSubPath();
            g.fillPath(speaker);
            juce::Path wave;
            wave.addCentredArc(x + w * 0.52f, cy, w * 0.26f, h * 0.26f, 0.0f, juce::MathConstants<float>::pi * 0.15f, juce::MathConstants<float>::pi * 0.85f, true);
            g.strokePath(wave, juce::PathStrokeType(stroke));
            break;
        }
        case HeaderIconButton::Icon::assets: // a grid of tiles
        {
            const auto cell = box.getWidth() * 0.42f;
            for (int row = 0; row < 2; ++row)
                for (int col = 0; col < 2; ++col)
                    g.fillRoundedRectangle(box.getX() + col * (box.getWidth() - cell), box.getY() + row * (box.getHeight() - cell), cell, cell, 2.0f);
            break;
        }
        case HeaderIconButton::Icon::pods: // a hexagon (a pod)
        {
            juce::Path hex;
            for (int i = 0; i < 6; ++i)
            {
                const auto angle = juce::MathConstants<float>::pi / 3.0f * (float) i - juce::MathConstants<float>::halfPi;
                const auto pt = juce::Point<float>(centre.x + std::cos(angle) * box.getWidth() * 0.5f, centre.y + std::sin(angle) * box.getHeight() * 0.5f);
                if (i == 0) hex.startNewSubPath(pt); else hex.lineTo(pt);
            }
            hex.closeSubPath();
            g.strokePath(hex, juce::PathStrokeType(stroke, juce::PathStrokeType::curved));
            g.fillEllipse(juce::Rectangle<float>(box.getWidth() * 0.22f, box.getWidth() * 0.22f).withCentre(centre));
            break;
        }
        case HeaderIconButton::Icon::tour: // a flag on a pole
        {
            const auto poleX = box.getX() + box.getWidth() * 0.22f;
            g.drawLine(poleX, box.getY(), poleX, box.getBottom(), stroke);
            juce::Path flag;
            flag.addTriangle(poleX, box.getY(), poleX, box.getY() + box.getHeight() * 0.55f, box.getRight(), box.getY() + box.getHeight() * 0.27f);
            g.fillPath(flag);
            break;
        }
        case HeaderIconButton::Icon::about: // a question mark
        {
            g.setFont(juce::Font(juce::FontOptions(size * 1.05f, juce::Font::bold)));
            g.drawText("?", area.toNearestInt(), juce::Justification::centred, false);
            break;
        }
        case HeaderIconButton::Icon::none:
            break;
    }
}

TransportButtonLookAndFeel& getTransportButtonLookAndFeel()
{
    static TransportButtonLookAndFeel lookAndFeel;
    return lookAndFeel;
}
}

CreationSuiteHeaderBar::CreationSuiteHeaderBar()
{
    setName("CreationSuiteHeaderBar");
    setAppLogo(creation::ui::SuiteLogoId::suite);

    titleLabel.setText("Djehuti Suite", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::Font(28.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    midiStatusLabel.setVisible(false);
    midiStatusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));

    projectButton.setButtonText("Project: Untitled");
    projectButton.setTooltip("Open the project menu");
    projectButton.setIcon(HeaderIconButton::Icon::project);
    projectButton.setFullLabel("Project: Untitled");
    addAndMakeVisible(projectButton);

    playButton.setLookAndFeel(&getTransportButtonLookAndFeel());
    pauseButton.setLookAndFeel(&getTransportButtonLookAndFeel());
    stopButton.setLookAndFeel(&getTransportButtonLookAndFeel());
    recordButton.setLookAndFeel(&getTransportButtonLookAndFeel());
    loopButton.setLookAndFeel(&getTransportButtonLookAndFeel());
    clickButton.setLookAndFeel(&getTransportButtonLookAndFeel());
    rewindButton.setLookAndFeel(&getTransportButtonLookAndFeel());
    fastForwardButton.setLookAndFeel(&getTransportButtonLookAndFeel());
    suiteButton.setLookAndFeel(&getTransportButtonLookAndFeel());

    recordButton.setName("recordButton");
    playButton.setButtonText("play");
    pauseButton.setButtonText("pause");
    stopButton.setButtonText("stop");
    recordButton.setButtonText("record");
    loopButton.setButtonText("loop");
    clickButton.setButtonText("click");
    rewindButton.setButtonText("next");
    fastForwardButton.setButtonText("prev");
    suiteButton.setButtonText("gear");

    playButton.setTooltip("Play");
    pauseButton.setTooltip("Pause");
    stopButton.setTooltip("Stop");
    recordButton.setTooltip("Record");
    loopButton.setTooltip("Loop");
    clickButton.setTooltip("Click");
    rewindButton.setTooltip("Rewind");
    fastForwardButton.setTooltip("Fast forward");

    auto configureTransportButton = [this](juce::Button& button)
    {
        button.setMouseClickGrabsKeyboardFocus(false);
        addAndMakeVisible(button);
    };

    configureTransportButton(playButton);
    configureTransportButton(pauseButton);
    configureTransportButton(stopButton);
    configureTransportButton(recordButton);
    configureTransportButton(loopButton);
    configureTransportButton(clickButton);
    configureTransportButton(rewindButton);
    configureTransportButton(fastForwardButton);

    playButton.addMouseListener(this, false);
    stopButton.addMouseListener(this, false);
    recordButton.addMouseListener(this, false);
    rewindButton.addMouseListener(this, false);
    fastForwardButton.addMouseListener(this, false);

    auto callback = [](auto& fn)
    {
        if (fn)
            fn();
    };

    playButton.onClick = [this, callback]
    {
        statusLabel.setText("Transport: play", juce::dontSendNotification);
        callback(onPlay);
    };
    pauseButton.onClick = [this, callback]
    {
        statusLabel.setText("Transport: pause", juce::dontSendNotification);
        callback(onPause);
    };
    stopButton.onClick = [this, callback]
    {
        statusLabel.setText("Transport: stop", juce::dontSendNotification);
        callback(onStop);
    };
    recordButton.onClick = [this, callback]
    {
        statusLabel.setText("Transport: record armed", juce::dontSendNotification);
        callback(onRecord);
    };
    rewindButton.onClick = [this, callback]
    {
        if (juce::ModifierKeys::currentModifiers.isCtrlDown() && onRewindToStart)
        {
            statusLabel.setText("Transport: rewind to start", juce::dontSendNotification);
            onRewindToStart();
            return;
        }

        statusLabel.setText("Transport: rewind", juce::dontSendNotification);
        callback(onRewind);
    };
    fastForwardButton.onClick = [this, callback]
    {
        if (juce::ModifierKeys::currentModifiers.isCtrlDown() && onFastForwardToEnd)
        {
            statusLabel.setText("Transport: fast forward to end", juce::dontSendNotification);
            onFastForwardToEnd();
            return;
        }

        statusLabel.setText("Transport: fast forward", juce::dontSendNotification);
        callback(onFastForward);
    };
    loopButton.setClickingTogglesState(true);
    loopButton.onClick = [this]
    {
        if (onLoopChanged)
            onLoopChanged(loopButton.getToggleState());
    };

    loopDelaySlider.setSliderStyle(juce::Slider::IncDecButtons);
    loopDelaySlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 50, 20);
    loopDelaySlider.setRange(0.0, 10.0, 0.1);
    loopDelaySlider.setValue(0.0, juce::dontSendNotification);
    loopDelaySlider.setTextValueSuffix("s");
    loopDelaySlider.setTooltip("Delay between loops");
    loopDelaySlider.onValueChange = [this]
    {
        if (onLoopDelayChanged)
            onLoopDelayChanged(loopDelaySlider.getValue());
    };
    addAndMakeVisible(loopDelaySlider);
    clickButton.setClickingTogglesState(false);
    clickButton.onClick = [this]
    {
        advanceMetronomeMode();
        if (onMetronomeModeChanged)
            onMetronomeModeChanged(metronomeMode);
    };

    signInButton.onClick = [this, callback] { callback(onSignInRequested); };
    signInButton.setTooltip("Sign in to sync your account");
    addAndMakeVisible(signInButton);

    projectButton.onClick = [this, callback] { callback(onProjectMenuRequested); };

    audioButton.onClick = [this, callback] { callback(onAudioRequested); };
    audioButton.setTooltip("Open audio settings");
    audioButton.setIcon(HeaderIconButton::Icon::audio);
    audioButton.setFullLabel(audioButton.getButtonText());
    addAndMakeVisible(audioButton);

    assetsButton.onClick = [this, callback] { callback(onAssetManagerRequested); };
    assetsButton.setTooltip("Open the Suite Asset Manager");
    assetsButton.setIcon(HeaderIconButton::Icon::assets);
    assetsButton.setFullLabel(assetsButton.getButtonText());
    addAndMakeVisible(assetsButton);

    podsButton.onClick = [this, callback] { callback(onPodsRequested); };
    podsButton.setTooltip("Browse Frate Pods");
    podsButton.setIcon(HeaderIconButton::Icon::pods);
    podsButton.setFullLabel(podsButton.getButtonText());
    addAndMakeVisible(podsButton);

    suiteButton.onClick = [this, callback] { callback(onSuiteRequested); };
    suiteButton.setTooltip("Open suite settings");
    addAndMakeVisible(suiteButton);

    // Deliberately not routed through a callback like the other utility
    // buttons above -- getInstance() reaches the running app directly, so
    // this works for every app that derives from SuiteJUCEApplication with
    // no per-app onAboutRequested wiring to remember.
    aboutButton.onClick = []
    {
        if (auto* app = dynamic_cast<creation::ui::SuiteJUCEApplication*>(juce::JUCEApplication::getInstance()))
            app->showAboutBox();
    };
    aboutButton.setTooltip("About this application");
    aboutButton.setIcon(HeaderIconButton::Icon::about);
    aboutButton.setFullLabel("About");
    addAndMakeVisible(aboutButton);

    tourButton.onClick = [this, callback] { callback(onTourRequested); };
    tourButton.setTooltip("Open guidance");
    tourButton.setIcon(HeaderIconButton::Icon::tour);
    tourButton.setFullLabel(tourButton.getButtonText());
    addAndMakeVisible(tourButton);

    profileNameLabel.setJustificationType(juce::Justification::centredLeft);
    profileNameLabel.setFont(juce::Font(14.0f).boldened());
    profileNameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(profileNameLabel);

    profileDetailLabel.setJustificationType(juce::Justification::centredLeft);
    profileDetailLabel.setFont(juce::Font(13.0f));
    profileDetailLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9fb0c8));
    addAndMakeVisible(profileDetailLabel);

    statusLabel.setText("Ready.", juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centredRight);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff99a6b8));
    addAndMakeVisible(statusLabel);

    scrubModeLabel.setText("SCRUB", juce::dontSendNotification);
    scrubModeLabel.setJustificationType(juce::Justification::centred);
    scrubModeLabel.setColour(juce::Label::textColourId, juce::Colour(0xff11151c));
    scrubModeLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xfff5c96a));
    scrubModeLabel.setVisible(false);
    addAndMakeVisible(scrubModeLabel);

    clearProfile();
    setScrubModeEnabled(false);
    setPlaybackVisualState(false, false);
}

CreationSuiteHeaderBar::~CreationSuiteHeaderBar()
{
    playButton.setLookAndFeel(nullptr);
    pauseButton.setLookAndFeel(nullptr);
    stopButton.setLookAndFeel(nullptr);
    recordButton.setLookAndFeel(nullptr);
    loopButton.setLookAndFeel(nullptr);
    clickButton.setLookAndFeel(nullptr);
    rewindButton.setLookAndFeel(nullptr);
    fastForwardButton.setLookAndFeel(nullptr);
}

void CreationSuiteHeaderBar::setAppTitle(const juce::String& title)
{
    titleLabel.setText(title, juce::dontSendNotification);
}

void CreationSuiteHeaderBar::setAppLogo(creation::ui::SuiteLogoId logoId)
{
    selectedLogoId = logoId;
    logoImage = creation::ui::getSuiteLogoImage(logoId);
    repaint();
}

void CreationSuiteHeaderBar::setSelectedLogoId(creation::ui::SuiteLogoId logoId)
{
    selectedLogoId = logoId;
    repaint();
}

void CreationSuiteHeaderBar::setLogoImage(const juce::Image& image)
{
    logoImage = image;
    repaint();
}

void CreationSuiteHeaderBar::setProfile(const ProfileData& profile)
{
    profileVisible = true;
    profileNameLabel.setVisible(true);
    profileDetailLabel.setVisible(true);
    signInButton.setVisible(false);

    profileNameLabel.setText(profile.displayName, juce::dontSendNotification);
    profileDetailLabel.setText(profile.detailText, juce::dontSendNotification);
    profileBadgeImage = profile.badgeImage;
    profileInitials = makeInitials(profile.displayName, profile.detailText);
    repaint();
}

void CreationSuiteHeaderBar::clearProfile()
{
    profileVisible = false;
    profileNameLabel.setText({}, juce::dontSendNotification);
    profileDetailLabel.setText({}, juce::dontSendNotification);
    profileBadgeImage = {};
    profileInitials.clear();
    profileNameLabel.setVisible(false);
    profileDetailLabel.setVisible(false);
    signInButton.setVisible(true);
    repaint();
}

void CreationSuiteHeaderBar::setProjectLabel(const juce::String& label)
{
    projectText = label;
    projectButton.setButtonText(label);
    projectButton.setFullLabel(label);
}

juce::Rectangle<int> CreationSuiteHeaderBar::getProjectButtonScreenBounds() const
{
    return localAreaToGlobal(projectButton.getBounds());
}

bool CreationSuiteHeaderBar::statusTextIsError(const juce::String& text)
{
    const auto lower = text.trim().toLowerCase();
    if (lower.isEmpty())
        return false;

    for (const char* opening : { "could not", "couldn't", "cannot", "can't", "unable to", "failed", "error" })
        if (lower.startsWith(opening))
            return true;

    return lower.contains("failed") || (lower.contains("error") && ! lower.contains("no error"));
}

void CreationSuiteHeaderBar::setStatusText(const juce::String& text)
{
    const auto isError = statusTextIsError(text);
    if (isError && onErrorStatus)
    {
        onErrorStatus(text);
        return;
    }

    if (! isError && onInfoStatus)
    {
        onInfoStatus(text);
        return;
    }

    statusLabel.setText(text, juce::dontSendNotification);
}

void CreationSuiteHeaderBar::setMidiStatusText(const juce::String& text)
{
    juce::ignoreUnused(text);
}

void CreationSuiteHeaderBar::setPlaybackVisualState(bool playing, bool recording)
{
    playbackIsPlaying = playing;
    playbackIsRecording = recording;
    refreshTransportButtonPresentation();
    refreshMetronomeButton();
}

void CreationSuiteHeaderBar::setMetronomeMode(MetronomeMode mode)
{
    metronomeMode = mode;
    refreshMetronomeButton();
}

void CreationSuiteHeaderBar::advanceMetronomeMode()
{
    switch (metronomeMode)
    {
        case MetronomeMode::off: metronomeMode = MetronomeMode::playOrRecord; break;
        case MetronomeMode::playOrRecord: metronomeMode = MetronomeMode::always; break;
        case MetronomeMode::always: metronomeMode = MetronomeMode::off; break;
    }

    refreshMetronomeButton();
}

void CreationSuiteHeaderBar::refreshMetronomeButton()
{
    metronomeAudible = metronomeMode == MetronomeMode::always
                        || (metronomeMode == MetronomeMode::playOrRecord && (playbackIsPlaying || playbackIsRecording));

    switch (metronomeMode)
    {
        case MetronomeMode::off:
            clickButton.setToggleState(false, juce::dontSendNotification);
            clickButton.setTooltip("Metronome off");
            statusLabel.setText("Transport: click off", juce::dontSendNotification);
            break;
        case MetronomeMode::playOrRecord:
            clickButton.setToggleState(true, juce::dontSendNotification);
            clickButton.setTooltip("Metronome on while playing or recording");
            statusLabel.setText("Transport: click on while playing or recording", juce::dontSendNotification);
            break;
        case MetronomeMode::always:
            clickButton.setToggleState(true, juce::dontSendNotification);
            clickButton.setTooltip("Metronome always on");
            statusLabel.setText("Transport: click always on", juce::dontSendNotification);
            break;
    }

    clickButton.repaint();
}

void CreationSuiteHeaderBar::setScrubModeEnabled(bool enabled)
{
    scrubModeEnabled = enabled;
    repaint();
}

void CreationSuiteHeaderBar::setTransportControlsVisible(bool shouldBeVisible)
{
    transportControlsVisible = shouldBeVisible;
    refreshTransportButtonPresentation();
    resized();
}

void CreationSuiteHeaderBar::setTransportButtonVisible(TransportButtonSlot slot, bool shouldBeVisible)
{
    auto& config = getTransportButtonConfig(slot);
    config.visible = shouldBeVisible;
    refreshTransportButtonPresentation();
    resized();
}

void CreationSuiteHeaderBar::setTransportButtonEnabled(TransportButtonSlot slot, bool shouldBeEnabled)
{
    auto& config = getTransportButtonConfig(slot);
    config.enabled = shouldBeEnabled;
    refreshTransportButtonPresentation();
}

void CreationSuiteHeaderBar::setSeparatePauseButtonVisible(bool shouldBeVisible)
{
    separatePauseButtonVisible = shouldBeVisible;
    refreshTransportButtonPresentation();
    resized();
}

void CreationSuiteHeaderBar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0f1115));
    if (logoImage.isValid())
        g.drawImageWithin(logoImage, 10, 4, 72, 72, juce::RectanglePlacement::centred, false);

    if (! logoRailBounds.isEmpty())
    {
        auto rail = logoRailBounds.toFloat();
        g.setColour(juce::Colour(0xff151b23));
        g.fillRoundedRectangle(rail, 12.0f);
        g.setColour(juce::Colour(0xff2b384a));
        g.drawRoundedRectangle(rail, 12.0f, 1.0f);

        auto ids = creation::ui::getSuiteLogoIds();
        auto tileWidth = rail.getWidth() / (float) ids.size();
        for (size_t index = 0; index < ids.size(); ++index)
        {
            auto id = ids[index];
            auto tile = juce::Rectangle<float>(rail.getX() + tileWidth * (float) index,
                                               rail.getY(),
                                               tileWidth,
                                               rail.getHeight()).reduced(4.0f, 4.0f);
            auto accent = creation::ui::getSuiteLogoAccentColour(id);

            if (id == selectedLogoId)
            {
                g.setColour(accent.withAlpha(0.18f));
                g.fillRoundedRectangle(tile, 10.0f);
                g.setColour(accent.withAlpha(0.92f));
                g.drawRoundedRectangle(tile, 10.0f, 1.8f);
            }

            auto icon = creation::ui::getSuiteLogoImage(id);
            if (icon.isValid())
                g.drawImageWithin(icon,
                                  (int) tile.getX(),
                                  (int) tile.getY(),
                                  (int) tile.getWidth(),
                                  (int) tile.getHeight(),
                                  juce::RectanglePlacement::centred,
                                  false);
        }
    }

    g.setColour(juce::Colour(0xff242a36));
    g.drawLine(0.0f, static_cast<float>(getHeight()) - 1.0f, static_cast<float>(getWidth()), static_cast<float>(getHeight()) - 1.0f, 1.0f);

    if (! transportControlBounds.isEmpty())
    {
        auto panel = transportControlBounds.toFloat().expanded(12.0f, 9.0f);
        auto accentGlowTop = scrubModeEnabled ? juce::Colour(0x55ff5b6e) : juce::Colour(0x4426d9ff);
        auto accentGlowBottom = scrubModeEnabled ? juce::Colour(0x001f0f14) : juce::Colour(0x00101820);
        juce::ColourGradient glow(accentGlowTop, panel.getCentreX(), panel.getY(),
                                  accentGlowBottom, panel.getCentreX(), panel.getBottom(), false);
        g.setGradientFill(glow);
        g.fillRoundedRectangle(panel.expanded(4.0f), 18.0f);

        g.setColour(juce::Colour(0xff151b23));
        g.fillRoundedRectangle(panel, 16.0f);
        g.setColour(scrubModeEnabled ? juce::Colour(0xffff5b6e) : juce::Colour(0xff36506b));
        g.drawRoundedRectangle(panel, 16.0f, scrubModeEnabled ? 2.6f : 1.4f);
    }

    if (profileVisible)
    {
        auto chip = profileChipBounds.toFloat();
        g.setColour(juce::Colour(0xff151b23));
        g.fillRoundedRectangle(chip, 15.0f);
        g.setColour(juce::Colour(0xff2c394c));
        g.drawRoundedRectangle(chip, 15.0f, 1.0f);

        auto avatar = chip.removeFromLeft(chip.getHeight()).reduced(3);
        g.setColour(juce::Colour(0xff223041));
        g.fillEllipse(avatar);
        g.setColour(juce::Colour(0xff8ea0b7));
        g.setFont(juce::Font(12.0f).boldened());
        g.drawText(profileInitials, avatar.toNearestInt(), juce::Justification::centred, false);

        auto badgeArea = chip.removeFromRight(30).withSizeKeepingCentre(22.0f, 22.0f);
        if (profileBadgeImage.isValid())
            g.drawImageWithin(profileBadgeImage, static_cast<int>(badgeArea.getX()), static_cast<int>(badgeArea.getY()),
                              static_cast<int>(badgeArea.getWidth()), static_cast<int>(badgeArea.getHeight()),
                              juce::RectanglePlacement::centred, false);
    }
}

void CreationSuiteHeaderBar::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
    {
        juce::String targetId;
        juce::String label;

        if (event.eventComponent == &rewindButton) { targetId = creation::ui::transport_actions::rewind; label = "Rewind"; }
        else if (event.eventComponent == &fastForwardButton) { targetId = creation::ui::transport_actions::fastForward; label = "Fast Forward"; }
        else if (event.eventComponent == &playButton) { targetId = creation::ui::transport_actions::play; label = "Play"; }
        else if (event.eventComponent == &stopButton) { targetId = creation::ui::transport_actions::stop; label = "Stop"; }
        else if (event.eventComponent == &recordButton) { targetId = creation::ui::transport_actions::record; label = "Record"; }

        if (targetId.isNotEmpty())
        {
            juce::PopupMenu menu;
            menu.addItem(1, "Learn MIDI Binding...");
            menu.showMenuAsync(juce::PopupMenu::Options(), [this, targetId, label](int result)
            {
                if (result == 1 && onLearnMidiRequested)
                    onLearnMidiRequested(targetId, label);
            });
            return;
        }
    }

    if (profileVisible && profileChipBounds.contains(event.getPosition()))
    {
        juce::PopupMenu menu;
        menu.addItem(1, "View profile page");
        menu.addSeparator();
        menu.addItem(2, "Log out");
        menu.showMenuAsync(juce::PopupMenu::Options(), [this](int result)
        {
            if (result == 1 && onOpenProfilePageRequested)
                onOpenProfilePageRequested();
            else if (result == 2 && onLogoutRequested)
                onLogoutRequested();
        });
    }
}

void CreationSuiteHeaderBar::resized()
{
    auto area = getLocalBounds().reduced(18, 10);
    area.removeFromLeft(82);

    auto topRow = area.removeFromTop(30);

    // The account chip sits at the end of the top row (it used to run down the whole right side). It is the first
    // thing to give up room in a narrow window and then keeps just its avatar.
    const int profileWidth = topRow.getWidth() >= 900 ? 190 : 44;
    auto profileArea = topRow.removeFromRight(profileWidth);
    topRow.removeFromRight(12);

    // Height is room, not size: the transport buttons stay the size they always were and the extra is the gap between
    // the two rows, so the transport group's frame does not ride up over the title row.
    if (area.getHeight() > 60)
        area.removeFromTop(12);
    auto bottomRow = area.withSizeKeepingCentre(area.getWidth(), juce::jmin(area.getHeight(), 42));

    // Nothing here ever disappears. When space runs short the controls change shape instead: first the
    // decorative logo rail goes, then the words on the buttons become icons (their names move to a hover tip),
    // and only then do the icons themselves get narrower.
    constexpr int utilityGap = 6;
    constexpr int projectGap = 10;
    constexpr int iconWidth = 34;
    constexpr int minimumProjectWidth = 140;
    constexpr int preferredProjectWidth = 190;
    constexpr int fullOthersWidth = 44 + 72 + 72 + 64 + 68 + 32; // suite, audio, assets, pods, tour, about
    constexpr int titleMinimum = 170;
    constexpr int titleFull = 290;
    constexpr int railFull = 248;
    constexpr int sectionGap = 10;

    const int fullNeed = minimumProjectWidth + projectGap + fullOthersWidth + 5 * utilityGap;
    const int mixedNeed = minimumProjectWidth + projectGap + 6 * iconWidth + 5 * utilityGap;
    const int iconNeed = iconWidth + projectGap + 6 * iconWidth + 5 * utilityGap;

    const int available = topRow.getWidth();
    bool projectCompact = false;
    bool othersCompact = false;
    int need = fullNeed;
    if (available < titleMinimum + sectionGap + fullNeed)
    {
        othersCompact = true;
        need = mixedNeed;
    }
    if (available < titleMinimum + sectionGap + mixedNeed)
    {
        projectCompact = true;
        need = iconNeed;
    }

    int buttonWidth = iconWidth;
    if (available < titleMinimum + sectionGap + iconNeed)
    {
        buttonWidth = juce::jmax(24, (available - 100 - projectGap - 5 * utilityGap) / 7);
        need = 7 * buttonWidth + projectGap + 5 * utilityGap;
    }

    const int spare = juce::jmax(0, available - need - sectionGap); // for the title and the decorative rail
    const int titleWidth = juce::jlimit(60, titleFull, spare);
    const int railWidth = spare - titleWidth >= 100 ? juce::jmin(railFull, spare - titleWidth) : 0;

    titleLabel.setBounds(topRow.removeFromLeft(titleWidth));
    topRow.removeFromLeft(sectionGap);
    logoRailBounds = railWidth > 0 ? topRow.removeFromLeft(railWidth) : juce::Rectangle<int>();

    auto utilityRow = topRow;
    const bool compactOthers = othersCompact || projectCompact;
    const int projectWidth = projectCompact ? buttonWidth
                                            : juce::jlimit(minimumProjectWidth, preferredProjectWidth,
                                                           minimumProjectWidth + juce::jmax(0, spare - titleFull - railFull));

    auto place = [&utilityRow](juce::Component& button, int width, int gapAfter)
    {
        button.setVisible(true);
        button.setBounds(utilityRow.removeFromLeft(juce::jmin(width, utilityRow.getWidth())));
        if (utilityRow.getWidth() > 0)
            utilityRow.removeFromLeft(juce::jmin(gapAfter, utilityRow.getWidth()));
    };

    projectButton.setCompact(projectCompact);
    audioButton.setCompact(compactOthers);
    assetsButton.setCompact(compactOthers);
    podsButton.setCompact(compactOthers);
    tourButton.setCompact(compactOthers);
    aboutButton.setCompact(compactOthers);

    place(projectButton, projectWidth, projectGap);
    place(suiteButton, compactOthers ? buttonWidth : 44, utilityGap);
    place(audioButton, compactOthers ? buttonWidth : 72, utilityGap);
    place(assetsButton, compactOthers ? buttonWidth : 72, utilityGap);
    place(podsButton, compactOthers ? buttonWidth : 64, utilityGap);
    place(tourButton, compactOthers ? buttonWidth : 68, utilityGap);
    place(aboutButton, compactOthers ? buttonWidth : 32, 0);

    // The status label is hidden by apps that route messages elsewhere; it must not keep reserving room then.
    if (statusLabel.isVisible())
        statusLabel.setBounds(bottomRow.removeFromRight(220));
    else
        statusLabel.setBounds({});
    auto transportRow = transportControlsVisible ? bottomRow : juce::Rectangle<int>();

    // The transport buttons scale down together when the row is short (they are icons, so they stay readable)
    // rather than the last ones being pushed off the end. Play must never be lost.
    const bool playShown = transportControlsVisible && playPauseButtonConfig.visible && (separatePauseButtonVisible || ! playbackIsPlaying);
    int widthSum = 0;
    int gapSum = 0;
    const auto count = [&](bool shown, int width) { if (shown) { widthSum += width; gapSum += 7; } };
    count(transportControlsVisible && rewindButtonConfig.visible, 62);
    count(transportControlsVisible && fastForwardButtonConfig.visible, 62);
    count(transportControlsVisible && stopButtonConfig.visible, 66);
    count(playShown, 82);
    count(separatePauseButtonVisible && transportControlsVisible && playPauseButtonConfig.visible, 82);
    count(transportControlsVisible && loopButtonConfig.visible, 64);
    if (transportControlsVisible && loopButtonConfig.visible)
    {
        widthSum += 75;
        gapSum += 12;
    }
    count(transportControlsVisible && clickButtonConfig.visible, 64);
    count(transportControlsVisible && recordButtonConfig.visible, 82);
    const double transportScale = widthSum > 0 && transportRow.getWidth() - gapSum < widthSum
                                      ? juce::jmax(0.0, (double) (transportRow.getWidth() - gapSum) / (double) widthSum)
                                      : 1.0;
    const auto scaled = [transportScale](int width) { return juce::jmax(34, (int) std::floor((double) width * transportScale)); };

    auto placeTransportButton = [&transportRow](juce::Button& button, int width, bool isVisible)
    {
        if (! isVisible)
        {
            button.setBounds({});
            button.setVisible(false);
            return;
        }

        button.setBounds(transportRow.removeFromLeft(width));
        transportRow.removeFromLeft(7);
        button.setVisible(true);
    };

    placeTransportButton(rewindButton, scaled(62), transportControlsVisible && rewindButtonConfig.visible);
    placeTransportButton(fastForwardButton, scaled(62), transportControlsVisible && fastForwardButtonConfig.visible);
    placeTransportButton(stopButton, scaled(66), transportControlsVisible && stopButtonConfig.visible);
    placeTransportButton(playButton, scaled(82), transportControlsVisible && playPauseButtonConfig.visible
                                           && (separatePauseButtonVisible || !playbackIsPlaying));
    if (separatePauseButtonVisible)
    {
        placeTransportButton(pauseButton, scaled(82), transportControlsVisible && playPauseButtonConfig.visible);
    }
    else
    {
        pauseButton.setBounds((transportControlsVisible && playPauseButtonConfig.visible) ? playButton.getBounds() : juce::Rectangle<int>());
        pauseButton.setVisible(transportControlsVisible && playPauseButtonConfig.visible && playbackIsPlaying);
    }
    placeTransportButton(loopButton, scaled(64), transportControlsVisible && loopButtonConfig.visible);
    if (transportControlsVisible && loopButtonConfig.visible)
    {
        transportRow.removeFromLeft(4);
        loopDelaySlider.setBounds(transportRow.removeFromLeft(juce::jmax(40, (int) std::floor(75.0 * transportScale))).reduced(0, 3));
        transportRow.removeFromLeft(8);
    }
    else
    {
        loopDelaySlider.setBounds({});
    }
    placeTransportButton(clickButton, scaled(64), transportControlsVisible && clickButtonConfig.visible);
    placeTransportButton(recordButton, scaled(82), transportControlsVisible && recordButtonConfig.visible);

    auto combineBounds = [](juce::Rectangle<int> left, juce::Rectangle<int> right, bool useRight)
    {
        if (left.isEmpty())
            return useRight ? right : juce::Rectangle<int>();

        return useRight ? left.getUnion(right) : left;
    };

    transportControlBounds = {};
    transportControlBounds = combineBounds(transportControlBounds, rewindButton.getBounds(), transportControlsVisible && rewindButtonConfig.visible);
    transportControlBounds = combineBounds(transportControlBounds, fastForwardButton.getBounds(), transportControlsVisible && fastForwardButtonConfig.visible);
    transportControlBounds = combineBounds(transportControlBounds, stopButton.getBounds(), transportControlsVisible && stopButtonConfig.visible);
    transportControlBounds = combineBounds(transportControlBounds, playButton.getBounds(), transportControlsVisible && playPauseButtonConfig.visible);
    transportControlBounds = combineBounds(transportControlBounds, loopButton.getBounds(), transportControlsVisible && loopButtonConfig.visible);
    transportControlBounds = combineBounds(transportControlBounds, clickButton.getBounds(), transportControlsVisible && clickButtonConfig.visible);
    transportControlBounds = combineBounds(transportControlBounds, recordButton.getBounds(), transportControlsVisible && recordButtonConfig.visible);

    signInButton.setBounds(profileArea);

    // One line: the name. The account's email is the hover text.
    profileNameLabel.setBounds(profileArea.withTrimmedLeft(36).withTrimmedRight(30));
    profileDetailLabel.setBounds({});
    profileNameLabel.setTooltip(profileDetailLabel.getText());
    profileChipBounds = profileArea;
    signInButton.setVisible(! profileVisible);
    profileNameLabel.setVisible(profileVisible && profileWidth > 100);
    profileDetailLabel.setVisible(false);
}

juce::String CreationSuiteHeaderBar::makeInitials(const juce::String& displayName, const juce::String& detailText)
{
    auto source = displayName.trim();
    if (source.isEmpty())
        source = detailText.upToFirstOccurrenceOf("@", false, false);

    juce::StringArray parts;
    parts.addTokens(source, " _-", {});
    juce::String initials;
    for (auto part : parts)
    {
        if (part.isNotEmpty())
            initials << part.substring(0, 1).toUpperCase();
        if (initials.length() >= 2)
            break;
    }

    if (initials.isEmpty() && source.isNotEmpty())
        initials = source.substring(0, 1).toUpperCase();

    return initials;
}

CreationSuiteHeaderBar::TransportButtonConfig& CreationSuiteHeaderBar::getTransportButtonConfig(TransportButtonSlot slot)
{
    switch (slot)
    {
        case TransportButtonSlot::rewind: return rewindButtonConfig;
        case TransportButtonSlot::fastForward: return fastForwardButtonConfig;
        case TransportButtonSlot::stop: return stopButtonConfig;
        case TransportButtonSlot::playPause: return playPauseButtonConfig;
        case TransportButtonSlot::loop: return loopButtonConfig;
        case TransportButtonSlot::click: return clickButtonConfig;
        case TransportButtonSlot::record: return recordButtonConfig;
    }

    return playPauseButtonConfig;
}

const CreationSuiteHeaderBar::TransportButtonConfig& CreationSuiteHeaderBar::getTransportButtonConfig(TransportButtonSlot slot) const
{
    switch (slot)
    {
        case TransportButtonSlot::rewind: return rewindButtonConfig;
        case TransportButtonSlot::fastForward: return fastForwardButtonConfig;
        case TransportButtonSlot::stop: return stopButtonConfig;
        case TransportButtonSlot::playPause: return playPauseButtonConfig;
        case TransportButtonSlot::loop: return loopButtonConfig;
        case TransportButtonSlot::click: return clickButtonConfig;
        case TransportButtonSlot::record: return recordButtonConfig;
    }

    return playPauseButtonConfig;
}

void CreationSuiteHeaderBar::refreshTransportButtonPresentation()
{
    rewindButton.setEnabled(rewindButtonConfig.enabled);
    fastForwardButton.setEnabled(fastForwardButtonConfig.enabled);
    stopButton.setEnabled(stopButtonConfig.enabled);
    playButton.setEnabled(playPauseButtonConfig.enabled && (!separatePauseButtonVisible || !playbackIsPlaying));
    pauseButton.setEnabled(playPauseButtonConfig.enabled && (!separatePauseButtonVisible || playbackIsPlaying));
    loopButton.setEnabled(loopButtonConfig.enabled);
    clickButton.setEnabled(clickButtonConfig.enabled);
    recordButton.setEnabled(recordButtonConfig.enabled);

    playButton.setVisible(transportControlsVisible && playPauseButtonConfig.visible
                          && (separatePauseButtonVisible || ! playbackIsPlaying));
    pauseButton.setVisible(transportControlsVisible && playPauseButtonConfig.visible
                           && (separatePauseButtonVisible || playbackIsPlaying));
    stopButton.setVisible(transportControlsVisible && stopButtonConfig.visible);
    rewindButton.setVisible(transportControlsVisible && rewindButtonConfig.visible);
    fastForwardButton.setVisible(transportControlsVisible && fastForwardButtonConfig.visible);
    loopButton.setVisible(transportControlsVisible && loopButtonConfig.visible);
    loopDelaySlider.setVisible(transportControlsVisible && loopButtonConfig.visible);
    clickButton.setVisible(transportControlsVisible && clickButtonConfig.visible);
    recordButton.setVisible(transportControlsVisible && recordButtonConfig.visible);

    playButton.setToggleState(playbackIsPlaying && ! playbackIsRecording, juce::dontSendNotification);
    pauseButton.setToggleState(false, juce::dontSendNotification);
    stopButton.setToggleState(! playbackIsPlaying && ! playbackIsRecording, juce::dontSendNotification);
    recordButton.setToggleState(playbackIsRecording, juce::dontSendNotification);

    repaint();
}

void HeaderIconButton::paintButton(juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    if (! compact || icon == Icon::none)
    {
        juce::TextButton::paintButton(g, isMouseOver, isButtonDown);
        return;
    }

    getLookAndFeel().drawButtonBackground(g, *this, findColour(getToggleState() ? buttonOnColourId : buttonColourId),
                                          isMouseOver, isButtonDown);

    g.setColour(findColour(getToggleState() ? textColourOnId : textColourOffId).withMultipliedAlpha(isEnabled() ? 1.0f : 0.5f));
    drawHeaderIcon(g, getLocalBounds().toFloat().reduced(7.0f, 6.0f), icon);
}
