#pragma once

#include <creation/ui/CreationSuiteLogos.h>
#include <juce_gui_extra/juce_gui_extra.h>

namespace creation::ui
{
class SuiteCommonSpacePanel final : public juce::Component
{
public:
    enum class Mode
    {
        splash,
        about
    };

    explicit SuiteCommonSpacePanel(Mode mode = Mode::splash);

    void setMode(Mode newMode);
    void setSelectedLogoId(SuiteLogoId logoId);
    void setStatusText(const juce::String& text);
    void setProgress(float progressValue);
    void setFooterText(const juce::String& text);
    void setPrimaryAction(const juce::String& buttonText, std::function<void()> onClick);
    void setSecondaryAction(const juce::String& buttonText, std::function<void()> onClick);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct HighlightArea
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    HighlightArea getHighlightArea() const noexcept;
    juce::Rectangle<float> getImageBounds() const;
    juce::Rectangle<float> getBottomBarBounds() const;
    void configureActionButton(juce::TextButton& button);

    Mode mode = Mode::splash;
    SuiteLogoId selectedLogoId = SuiteLogoId::suite;
    juce::Image backgroundImage;
    juce::String statusText { "Loading Creation Suite..." };
    juce::String footerText;
    float progress = 0.0f;
    juce::TextButton primaryActionButton;
    juce::TextButton secondaryActionButton;
    std::function<void()> primaryAction;
    std::function<void()> secondaryAction;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SuiteCommonSpacePanel)
};
}
