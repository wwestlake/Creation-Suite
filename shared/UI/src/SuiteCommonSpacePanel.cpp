#include <creation/ui/SuiteCommonSpacePanel.h>
#include <BinaryData.h>
#include <algorithm>
#include <vector>

namespace
{
std::vector<creation::ui::SuiteLogoId> getAppLogoIds()
{
    return {
        creation::ui::SuiteLogoId::texture,
        creation::ui::SuiteLogoId::modeler,
        creation::ui::SuiteLogoId::station,
        creation::ui::SuiteLogoId::engine,
        creation::ui::SuiteLogoId::engineer,
        creation::ui::SuiteLogoId::movie,
        creation::ui::SuiteLogoId::live
    };
}

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

juce::Rectangle<float> getSuiteHeroBounds(juce::Rectangle<float> imageBounds)
{
    auto width = juce::jmin(imageBounds.getWidth() * 0.34f, 320.0f);
    auto height = juce::jmin(imageBounds.getHeight() * 0.34f, 220.0f);
    auto x = imageBounds.getCentreX() - width * 0.5f;
    auto y = imageBounds.getY() + imageBounds.getHeight() * 0.055f;
    return { x, y, width, height };
}

juce::Rectangle<float> getAppGridBounds(juce::Rectangle<float> imageBounds)
{
    auto top = getSuiteHeroBounds(imageBounds).getBottom() + imageBounds.getHeight() * 0.045f;
    auto bottomPadding = imageBounds.getHeight() * 0.045f;
    return imageBounds.withTrimmedTop(top - imageBounds.getY())
                      .withTrimmedBottom(bottomPadding);
}

juce::Rectangle<float> getLogoCardBounds(creation::ui::SuiteLogoId logoId,
                                         juce::Rectangle<float> imageBounds) noexcept
{
    if (logoId == creation::ui::SuiteLogoId::suite)
        return getSuiteHeroBounds(imageBounds);

    const auto logos = getAppLogoIds();
    const auto found = std::find(logos.begin(), logos.end(), logoId);
    if (found == logos.end())
        return {};

    auto grid = getAppGridBounds(imageBounds);
    constexpr int columns = 4;
    constexpr float gap = 18.0f;

    const auto index = static_cast<int>(std::distance(logos.begin(), found));
    const auto rows = static_cast<int>((logos.size() + columns - 1) / columns);
    const auto row = index / columns;
    const auto indexInRow = index % columns;
    const auto itemsInRow = row == rows - 1 ? static_cast<int>(logos.size()) - row * columns : columns;

    auto cardWidth = (grid.getWidth() - gap * (columns - 1)) / static_cast<float>(columns);
    auto cardHeight = (grid.getHeight() - gap * juce::jmax(0, rows - 1)) / static_cast<float>(rows);
    auto rowWidth = cardWidth * itemsInRow + gap * juce::jmax(0, itemsInRow - 1);
    auto rowStartX = grid.getCentreX() - rowWidth * 0.5f;
    auto x = rowStartX + indexInRow * (cardWidth + gap);
    auto y = grid.getY() + row * (cardHeight + gap);
    return { x, y, cardWidth, cardHeight };
}

juce::Rectangle<float> getIconBounds(juce::Rectangle<float> cardBounds)
{
    auto iconSize = juce::jmin(cardBounds.getHeight() * 0.66f, cardBounds.getWidth() * 0.26f);
    auto x = cardBounds.getX() + 18.0f;
    auto y = cardBounds.getCentreY() - iconSize * 0.5f;
    return { x, y, iconSize, iconSize };
}

void drawCard(juce::Graphics& g,
              juce::Rectangle<float> bounds,
              creation::ui::SuiteLogoId logoId,
              bool selected)
{
    auto accent = creation::ui::getSuiteLogoAccentColour(logoId);
    const auto cardAlpha = selected ? 1.0f : 0.5f;
    auto panelColour = juce::Colour(0xd9151c27).withMultipliedAlpha(cardAlpha);

    if (selected)
    {
        juce::DropShadow glow(accent.withAlpha(0.34f), 18, { 0, 0 });
        glow.drawForRectangle(g, bounds.expanded(8.0f, 6.0f).toNearestInt());
    }

    g.setColour(panelColour);
    g.fillRoundedRectangle(bounds, 16.0f);
    g.setColour(selected ? accent.withAlpha(0.80f)
                         : juce::Colour(0x55394b61).withMultipliedAlpha(0.5f));
    g.drawRoundedRectangle(bounds, 16.0f, selected ? 2.0f : 1.0f);

    auto iconBounds = getIconBounds(bounds);
    auto icon = creation::ui::getSuiteLogoImage(logoId);
    if (icon.isValid())
    {
        g.setOpacity(cardAlpha);
        g.drawImageWithin(icon,
                          juce::roundToInt(iconBounds.getX()),
                          juce::roundToInt(iconBounds.getY()),
                          juce::roundToInt(iconBounds.getWidth()),
                          juce::roundToInt(iconBounds.getHeight()),
                          juce::RectanglePlacement::centred,
                          false);
        g.setOpacity(1.0f);
    }

    auto textArea = bounds;
    textArea.removeFromLeft(iconBounds.getRight() - bounds.getX() + 16.0f);
    textArea.reduce(0.0f, 14.0f);

    g.setColour(juce::Colour(0xfff4f7fb).withMultipliedAlpha(cardAlpha));
    g.setFont(juce::Font(13.0f).boldened());
    g.drawText("CREATION",
               textArea.removeFromTop(20.0f).toNearestInt(),
               juce::Justification::centredLeft,
               true);

    g.setColour((selected ? accent.brighter(0.05f) : accent.withMultipliedSaturation(0.85f))
                    .withMultipliedAlpha(cardAlpha));
    g.setFont(juce::Font(22.0f).boldened());

    auto name = creation::ui::getSuiteLogoDisplayName(logoId);
    auto shortName = name.fromFirstOccurrenceOf("Creation ", false, false).toUpperCase();
    g.drawText(shortName,
               textArea.toNearestInt(),
               juce::Justification::centredLeft,
               true);
}

void drawSuiteHero(juce::Graphics& g,
                   juce::Rectangle<float> bounds,
                   bool selected)
{
    auto accent = creation::ui::getSuiteLogoAccentColour(creation::ui::SuiteLogoId::suite);

    juce::DropShadow heroGlow(accent.withAlpha(selected ? 0.36f : 0.24f), 24, { 0, 0 });
    heroGlow.drawForRectangle(g, bounds.expanded(10.0f, 10.0f).toNearestInt());

    g.setGradientFill(juce::ColourGradient(juce::Colour(0xbf111824), bounds.getX(), bounds.getY(),
                                           juce::Colour(0x8c0c1018), bounds.getRight(), bounds.getBottom(), false));
    g.fillRoundedRectangle(bounds, 24.0f);
    g.setColour(selected ? accent.withAlpha(0.75f) : juce::Colour(0x55394b61));
    g.drawRoundedRectangle(bounds, 24.0f, selected ? 2.0f : 1.0f);

    auto content = bounds.reduced(22.0f, 16.0f);
    auto iconArea = content.removeFromTop(content.getHeight() * 0.63f);
    auto icon = creation::ui::getSuiteLogoImage(creation::ui::SuiteLogoId::suite);
    if (icon.isValid())
        g.drawImageWithin(icon,
                          juce::roundToInt(iconArea.getX()),
                          juce::roundToInt(iconArea.getY()),
                          juce::roundToInt(iconArea.getWidth()),
                          juce::roundToInt(iconArea.getHeight()),
                          juce::RectanglePlacement::centred,
                          false);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(17.0f).boldened());
    g.drawText("CREATION",
               content.removeFromTop(24.0f).toNearestInt(),
               juce::Justification::centred,
               true);

    g.setColour(accent);
    g.setFont(juce::Font(28.0f).boldened());
    g.drawText("SUITE",
               content.toNearestInt(),
               juce::Justification::centred,
               true);
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

    g.setGradientFill(juce::ColourGradient(juce::Colour(0x46050a10), imageBounds.getCentreX(), imageBounds.getY(),
                                           juce::Colour(0xb0141921), imageBounds.getCentreX(), imageBounds.getBottom(), false));
    g.fillRect(imageBounds);

    auto suiteBounds = getLogoCardBounds(SuiteLogoId::suite, imageBounds);
    drawSuiteHero(g, suiteBounds, selectedLogoId == SuiteLogoId::suite);

    for (auto logoId : getAppLogoIds())
        drawCard(g, getLogoCardBounds(logoId, imageBounds), logoId, selectedLogoId == logoId);

    auto taglineBounds = imageBounds.withTrimmedTop(imageBounds.getHeight() * 0.90f)
                                   .reduced(24.0f, 0.0f);
    g.setColour(juce::Colour(0x99f6d79c));
    g.setFont(juce::Font(20.0f, juce::Font::italic));
    g.drawFittedText("Beyond here there be dragons.",
                     taglineBounds.toNearestInt(),
                     juce::Justification::centred,
                     1);

    auto bottomBar = getBottomBarBounds();
    auto accent = getSuiteLogoAccentColour(selectedLogoId);
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
