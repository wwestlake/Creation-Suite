#include <creation/ui/SuiteEulaPanel.h>
#include <creation/services/SuiteEula.h>

namespace
{
juce::Colour panelFill() { return juce::Colour(0xff121822); }
juce::Colour panelOutline() { return juce::Colour(0xff2a3a50); }
juce::Colour hintColour() { return juce::Colour(0xff97a9c1); }
}

SuiteEulaPanel::SuiteEulaPanel()
{
    titleLabel.setText("Creation Suite EULA", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(24.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    versionLabel.setText("Version: " + creation::services::SuiteEula::currentVersion(), juce::dontSendNotification);
    versionLabel.setColour(juce::Label::textColourId, hintColour());
    addAndMakeVisible(versionLabel);

    textEditor.setMultiLine(true);
    textEditor.setReadOnly(true);
    textEditor.setScrollbarsShown(true);
    textEditor.setCaretVisible(false);
    textEditor.setText(creation::services::SuiteEula::currentText(), juce::dontSendNotification);
    textEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1a2230));
    textEditor.setColour(juce::TextEditor::outlineColourId, panelOutline());
    textEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    addAndMakeVisible(textEditor);
}

void SuiteEulaPanel::paint(juce::Graphics& g)
{
    g.fillAll(panelFill());
    g.setColour(panelOutline());
    g.drawRect(getLocalBounds(), 1);
}

void SuiteEulaPanel::resized()
{
    auto area = getLocalBounds().reduced(18);
    titleLabel.setBounds(area.removeFromTop(30));
    versionLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(8);
    textEditor.setBounds(area);
}
