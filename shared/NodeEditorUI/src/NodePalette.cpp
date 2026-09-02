#include <creation/node_editor_ui/NodePalette.h>

#include <algorithm>

namespace creation::node_editor_ui {

NodePalette::NodePalette(const ce::node_system::NodeTypeRegistry& registry) : listBox_({}, this) {
    for (const auto& [typeName, descriptor] : registry.Types()) {
        entries_.push_back({typeName, descriptor.displayName.empty() ? typeName : descriptor.displayName, descriptor.category});
    }
    std::sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) {
        return a.category != b.category ? a.category < b.category : a.displayName < b.displayName;
    });

    titleLabel_.setFont(juce::Font(juce::FontOptions(15.0f)).boldened());
    titleLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel_);

    listBox_.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff181c22));
    listBox_.setRowHeight(24);
    addAndMakeVisible(listBox_);
}

int NodePalette::getNumRows() {
    return static_cast<int>(entries_.size());
}

void NodePalette::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) {
    if (rowNumber < 0 || rowNumber >= static_cast<int>(entries_.size())) {
        return;
    }
    if (rowIsSelected) {
        g.fillAll(juce::Colour(0xff2a3644));
    }
    const auto& entry = entries_[static_cast<std::size_t>(rowNumber)];
    constexpr int categoryColumnWidth = 66;
    if (!entry.category.empty()) {
        g.setColour(juce::Colour(0xff6b7a8c));
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText(entry.category, 8, 0, categoryColumnWidth, height, juce::Justification::centredLeft, true);
    }
    g.setColour(juce::Colour(0xffb8c4d5));
    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    g.drawText(entry.displayName, 8 + categoryColumnWidth, 0, width - 8 - categoryColumnWidth, height,
               juce::Justification::centredLeft, true);
}

juce::var NodePalette::getDragSourceDescription(const juce::SparseSet<int>& selectedRows) {
    if (selectedRows.isEmpty()) {
        return {};
    }
    const int row = selectedRows[0];
    if (row < 0 || row >= static_cast<int>(entries_.size())) {
        return {};
    }
    return juce::String(entries_[static_cast<std::size_t>(row)].typeName);
}

void NodePalette::resized() {
    auto bounds = getLocalBounds();
    titleLabel_.setBounds(bounds.removeFromTop(24));
    listBox_.setBounds(bounds);
}

void NodePalette::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff15181d));
}

} // namespace creation::node_editor_ui
