#include <creation/ui/SuiteCommonSpacePanel.h>
#include <BinaryData.h>

namespace
{
struct HighlightGeometry
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

juce::Image loadBannerFromRepository()
{
    auto executable = juce::File::getSpecialLocation(juce::File::currentExecutableFile);

    for (auto dir = executable.getParentDirectory(); dir.exists(); dir = dir.getParentDirectory())
    {
        auto candidate = dir.getChildFile("shared").getChildFile("UI").getChildFile("assets").getChildFile("creation_suite_banner.png");
        if (candidate.existsAsFile())
            return juce::ImageFileFormat::loadFrom(candidate);

        auto parent = dir.getParentDirectory();
        if (parent == dir)
            break;
    }

    return {};
}

juce::Image loadBackgroundImage()
{
    juce::PNGImageFormat pngFormat;
    juce::MemoryInputStream stream(BinaryData::creation_suite_banner_png,
                                   static_cast<size_t>(BinaryData::creation_suite_banner_pngSize),
                                   false);

    auto image = pngFormat.decodeImage(stream);
    if (image.isValid())
        return image;

    return loadBannerFromRepository();
}

juce::Rectangle<float> scaleNormalizedRect(juce::Rectangle<float> bounds,
                                           float x,
                                           float y,
                                           float width,
                                           float height)
{
    return { bounds.getX() + bounds.getWidth() * x,
             bounds.getY() + bounds.getHeight() * y,
             bounds.getWidth() * width,
             bounds.getHeight() * height };
}

constexpr float kAppHighlightXPadding = 0.010f;
constexpr float kAppHighlightYPadding = 0.010f;
constexpr float kAppHighlightWidth = 0.292f;
constexpr float kAppHighlightHeight = 0.236f;

HighlightGeometry makeAppHighlight(float x, float y)
{
    return { x - kAppHighlightXPadding,
             y - kAppHighlightYPadding,
             kAppHighlightWidth,
             kAppHighlightHeight };
}

HighlightGeometry getAppHighlightGeometry(creation::ui::SuiteLogoId logoId) noexcept
{
    switch (logoId)
    {
        case creation::ui::SuiteLogoId::texture: return makeAppHighlight(0.042f, 0.505f);
        case creation::ui::SuiteLogoId::modeler: return makeAppHighlight(0.356f, 0.505f);
        case creation::ui::SuiteLogoId::station: return makeAppHighlight(0.646f, 0.505f);
        case creation::ui::SuiteLogoId::engine: return makeAppHighlight(0.042f, 0.748f);
        case creation::ui::SuiteLogoId::movie: return makeAppHighlight(0.356f, 0.748f);
        case creation::ui::SuiteLogoId::live: return makeAppHighlight(0.646f, 0.748f);
        case creation::ui::SuiteLogoId::suite: break;
    }

    return {};
}

}

namespace creation::ui
{
SuiteCommonSpacePanel::SuiteCommonSpacePanel(Mode initialMode)
    : mode(initialMode),
      backgroundImage(loadBackgroundImage()),
      primaryActionButton("Open"),
      secondaryActionButton("Later")
{
    setOpaque(true);
    configureActionButton(primaryActionButton);
    configureActionButton(secondaryActionButton);
    setSecondaryAction({}, {});
    setPrimaryAction({}, {});
}

void SuiteCommonSpacePanel::setMode(Mode newMode)
{
    mode = newMode;
    repaint();
}

void SuiteCommonSpacePanel::setSelectedLogoId(SuiteLogoId logoId)
{
    selectedLogoId = logoId;
    repaint();
}

void SuiteCommonSpacePanel::setStatusText(const juce::String& text)
{
    statusText = text;
    repaint();
}

void SuiteCommonSpacePanel::setProgress(float progressValue)
{
    progress = juce::jlimit(0.0f, 1.0f, progressValue);
    repaint();
}

void SuiteCommonSpacePanel::setFooterText(const juce::String& text)
{
    footerText = text;
    repaint();
}

void SuiteCommonSpacePanel::setPrimaryAction(const juce::String& buttonText, std::function<void()> onClick)
{
    primaryAction = std::move(onClick);
    primaryActionButton.setButtonText(buttonText);
    primaryActionButton.setVisible(buttonText.isNotEmpty() && primaryAction != nullptr);
}

void SuiteCommonSpacePanel::setSecondaryAction(const juce::String& buttonText, std::function<void()> onClick)
{
    secondaryAction = std::move(onClick);
    secondaryActionButton.setButtonText(buttonText);
    secondaryActionButton.setVisible(buttonText.isNotEmpty() && secondaryAction != nullptr);
}

void SuiteCommonSpacePanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0b0f14));

    auto imageBounds = getImageBounds();
    if (backgroundImage.isValid())
    {
        g.drawImageWithin(backgroundImage,
                          juce::roundToInt(imageBounds.getX()),
                          juce::roundToInt(imageBounds.getY()),
                          juce::roundToInt(imageBounds.getWidth()),
                          juce::roundToInt(imageBounds.getHeight()),
                          juce::RectanglePlacement::stretchToFit,
                          false);
    }

    auto accent = getSuiteLogoAccentColour(selectedLogoId);
    auto highlight = getHighlightArea();
    auto selectedArea = scaleNormalizedRect(imageBounds,
                                            highlight.x,
                                            highlight.y,
                                            highlight.width,
                                            highlight.height);

    juce::DropShadow glow(accent.withAlpha(0.22f), 16, { 0, 0 });
    glow.drawForRectangle(g, selectedArea.expanded(8.0f, 5.0f).toNearestInt());
    g.setColour(accent.withAlpha(0.015f));
    g.fillRoundedRectangle(selectedArea.expanded(3.0f, 2.0f), 12.0f);
    g.setColour(accent.withAlpha(0.16f));
    g.drawRoundedRectangle(selectedArea.expanded(1.0f, 1.0f), 11.0f, 1.0f);

    auto bottomBar = getBottomBarBounds();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xdd081018), bottomBar.getX(), bottomBar.getY(),
                                           juce::Colour(0xf0151b24), bottomBar.getX(), bottomBar.getBottom(), false));
    g.fillRoundedRectangle(bottomBar, 18.0f);
    g.setColour(juce::Colour(0xff27364a));
    g.drawRoundedRectangle(bottomBar, 18.0f, 1.0f);

    auto content = bottomBar.reduced(22.0f, 18.0f);
    auto left = content.removeFromLeft(560.0f);
    auto right = content;

    auto chipIconArea = left.removeFromLeft(44.0f).reduced(2.0f);
    auto selectedIcon = getSuiteLogoImage(selectedLogoId);
    if (selectedIcon.isValid())
        g.drawImageWithin(selectedIcon,
                          (int) chipIconArea.getX(),
                          (int) chipIconArea.getY(),
                          (int) chipIconArea.getWidth(),
                          (int) chipIconArea.getHeight(),
                          juce::RectanglePlacement::centred,
                          false);

    left.removeFromLeft(14.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(24.0f).boldened());
    g.drawText(mode == Mode::splash ? "Loading " + getSuiteLogoDisplayName(selectedLogoId)
                                    : getSuiteLogoDisplayName(selectedLogoId),
               left.removeFromTop(28.0f).toNearestInt(),
               juce::Justification::centredLeft,
               true);

    g.setColour(juce::Colour(0xffb4c1d3));
    g.setFont(juce::Font(16.0f));
    g.drawText(statusText.isNotEmpty() ? statusText : "Preparing the suite surface...",
               left.removeFromTop(24.0f).toNearestInt(),
               juce::Justification::centredLeft,
               true);

    auto progressArea = left.removeFromTop(10.0f);
    progressArea.setHeight(8.0f);
    g.setColour(juce::Colour(0x55415b78));
    g.fillRoundedRectangle(progressArea, 4.0f);
    g.setGradientFill(juce::ColourGradient(accent.brighter(0.15f), progressArea.getX(), progressArea.getY(),
                                           accent.darker(0.15f), progressArea.getRight(), progressArea.getY(), false));
    g.fillRoundedRectangle(progressArea.withWidth(progressArea.getWidth() * progress), 4.0f);

    auto footerArea = left.removeFromTop(22.0f);
    g.setColour(juce::Colour(0xffd7e1f3));
    g.setFont(juce::Font(14.0f));
    auto footer = footerText.isNotEmpty()
                    ? footerText
                    : juce::String(juce::roundToInt(progress * 100.0f)) + "% complete";
    g.drawText(footer, footerArea.toNearestInt(), juce::Justification::centredLeft, true);

    if (primaryActionButton.isVisible() || secondaryActionButton.isVisible())
    {
        g.setColour(juce::Colour(0xff7f91a7));
        g.setFont(juce::Font(13.0f));
        g.drawText("Actions", right.removeFromTop(20).toNearestInt(), juce::Justification::centredRight, true);
    }
}

void SuiteCommonSpacePanel::resized()
{
    auto bottomBar = getBottomBarBounds().reduced(22.0f, 18.0f).toNearestInt();
    auto right = bottomBar.removeFromRight(230);

    primaryActionButton.setBounds(right.removeFromTop(34));
    right.removeFromTop(8);
    secondaryActionButton.setBounds(right.removeFromTop(30));
}

SuiteCommonSpacePanel::HighlightArea SuiteCommonSpacePanel::getHighlightArea() const noexcept
{
    switch (selectedLogoId)
    {
        case SuiteLogoId::suite: return { 0.404f, 0.050f, 0.192f, 0.255f };
        case SuiteLogoId::texture:
        case SuiteLogoId::modeler:
        case SuiteLogoId::station:
        case SuiteLogoId::engine:
        case SuiteLogoId::movie:
        case SuiteLogoId::live:
        {
            const auto highlight = getAppHighlightGeometry(selectedLogoId);
            return { highlight.x, highlight.y, highlight.width, highlight.height };
        }
    }

    return {};
}

juce::Rectangle<float> SuiteCommonSpacePanel::getImageBounds() const
{
    auto bounds = getLocalBounds().toFloat().reduced(16.0f);
    auto bottomBar = getBottomBarBounds();
    return bounds.withBottom(bottomBar.getY() - 12.0f);
}

juce::Rectangle<float> SuiteCommonSpacePanel::getBottomBarBounds() const
{
    auto bounds = getLocalBounds().toFloat().reduced(16.0f);
    auto height = juce::jmax(92.0f, juce::jmin(132.0f, bounds.getHeight() * 0.22f));
    return bounds.removeFromBottom(height);
}

void SuiteCommonSpacePanel::configureActionButton(juce::TextButton& button)
{
    button.setVisible(false);
    button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff13202b));
    button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff1d3041));
    button.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    addAndMakeVisible(button);

    if (&button == &primaryActionButton)
    {
        button.onClick = [this]
        {
            if (primaryAction)
                primaryAction();
        };
    }
    else
    {
        button.onClick = [this]
        {
            if (secondaryAction)
                secondaryAction();
        };
    }
}
}
