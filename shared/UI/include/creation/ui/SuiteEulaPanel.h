#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

class SuiteEulaPanel final : public juce::Component
{
public:
    SuiteEulaPanel();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Label titleLabel;
    juce::Label versionLabel;
    juce::TextEditor textEditor;
};
