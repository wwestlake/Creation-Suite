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
    for (const auto& entry : entries_) {
        categoryExpanded_.emplace(entry.category, true);
    }

    titleLabel_.setFont(juce::Font(juce::FontOptions(15.0f)).boldened());
    titleLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel_);

    filterBox_.setTextToShowWhenEmpty("Filter...", juce::Colour(0xff6b7a8c));
    filterBox_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff20262f));
    filterBox_.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    filterBox_.onTextChange = [this] {
        RebuildRows();
        listBox_.updateContent();
        repaint();
    };
    addAndMakeVisible(filterBox_);

    listBox_.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff181c22));
    listBox_.setRowHeight(24);
    addAndMakeVisible(listBox_);

    RebuildRows();
}

void NodePalette::RebuildRows() {
    rows_.clear();
    const juce::String filter = filterBox_.getText().trim();
    const bool filtering = filter.isNotEmpty();

    std::size_t i = 0;
    while (i < entries_.size()) {
        const std::string category = entries_[i].category;
        std::vector<std::size_t> matchingIndices;
        std::size_t j = i;
        while (j < entries_.size() && entries_[j].category == category) {
            const Entry& entry = entries_[j];
            const bool matches = !filtering || juce::String(entry.displayName).containsIgnoreCase(filter) ||
                                  juce::String(entry.typeName).containsIgnoreCase(filter);
            if (matches) {
                matchingIndices.push_back(j);
            }
            ++j;
        }
        if (!matchingIndices.empty()) {
            Row header;
            header.isHeader = true;
            header.category = category;
            header.count = static_cast<int>(matchingIndices.size());
            rows_.push_back(header);

            const bool expanded = categoryExpanded_[category] || filtering;
            if (expanded) {
                for (const auto idx : matchingIndices) {
                    Row row;
                    row.entryIndex = static_cast<int>(idx);
                    rows_.push_back(row);
                }
            }
        }
        i = j;
    }
}

int NodePalette::getNumRows() {
    return static_cast<int>(rows_.size());
}

void NodePalette::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) {
    if (rowNumber < 0 || rowNumber >= static_cast<int>(rows_.size())) {
        return;
    }
    const Row& row = rows_[static_cast<std::size_t>(rowNumber)];

    if (row.isHeader) {
        g.fillAll(juce::Colour(0xff20262f));
        const bool expanded = categoryExpanded_[row.category] || filterBox_.getText().trim().isNotEmpty();
        const juce::String label = (row.category.empty() ? juce::String("Other") : juce::String(row.category)) +
                                    "  (" + juce::String(row.count) + ")";
        g.setColour(juce::Colour(0xff9aa8ba));
        g.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
        g.drawText((expanded ? juce::String(juce::CharPointer_UTF8("\xe2\x96\xbc ")) : juce::String(juce::CharPointer_UTF8("\xe2\x96\xb6 "))) + label,
                   8, 0, width - 8, height, juce::Justification::centredLeft, true);
        return;
    }

    if (row.entryIndex < 0 || row.entryIndex >= static_cast<int>(entries_.size())) {
        return;
    }
    if (rowIsSelected) {
        g.fillAll(juce::Colour(0xff2a3644));
    }
    const auto& entry = entries_[static_cast<std::size_t>(row.entryIndex)];
    g.setColour(juce::Colour(0xffb8c4d5));
    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    g.drawText(entry.displayName, 20, 0, width - 20, height, juce::Justification::centredLeft, true);
}

void NodePalette::listBoxItemClicked(int row, const juce::MouseEvent&) {
    if (row < 0 || row >= static_cast<int>(rows_.size())) {
        return;
    }
    if (!rows_[static_cast<std::size_t>(row)].isHeader) {
        return;
    }
    bool& expanded = categoryExpanded_[rows_[static_cast<std::size_t>(row)].category];
    expanded = !expanded;
    RebuildRows();
    listBox_.updateContent();
    listBox_.deselectAllRows();
    repaint();
}

juce::var NodePalette::getDragSourceDescription(const juce::SparseSet<int>& selectedRows) {
    if (selectedRows.isEmpty()) {
        return {};
    }
    const int row = selectedRows[0];
    if (row < 0 || row >= static_cast<int>(rows_.size())) {
        return {};
    }
    const Row& r = rows_[static_cast<std::size_t>(row)];
    if (r.isHeader || r.entryIndex < 0 || r.entryIndex >= static_cast<int>(entries_.size())) {
        return {};
    }
    return juce::String(entries_[static_cast<std::size_t>(r.entryIndex)].typeName);
}

void NodePalette::resized() {
    auto bounds = getLocalBounds();
    titleLabel_.setBounds(bounds.removeFromTop(24));
    filterBox_.setBounds(bounds.removeFromTop(26).reduced(4, 2));
    listBox_.setBounds(bounds);
}

void NodePalette::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff15181d));
}

} // namespace creation::node_editor_ui
