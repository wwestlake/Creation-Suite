#include <creation/node_editor_ui/NodePalette.h>

#include <algorithm>

namespace creation::node_editor_ui {

NodePalette::NodePalette(const ce::node_system::NodeTypeRegistry& registry) : registry_(registry), listBox_({}, this) {
    RefreshFromRegistry();

    titleLabel_.setFont(juce::Font(juce::FontOptions(15.0f)).boldened());
    titleLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel_);

    filterBox_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff20262f));
    filterBox_.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    filterBox_.setTextToShowWhenEmpty("Search nodes - name, category or what they do", juce::Colour(0xff6b7a8c));
    filterBox_.onTextChange = [this] {
        RebuildRows();
        listBox_.updateContent();
        // Results start at the top, the best one selected (Enter adds it).
        listBox_.scrollToEnsureRowIsOnscreen(0);
        if (filterBox_.getText().trim().isNotEmpty() && !rows_.empty())
            listBox_.selectRow(0);
        else
            listBox_.deselectAllRows();
        repaint();
    };
    filterBox_.onReturnKey = [this] {
        const int selected = listBox_.getSelectedRow();
        AddRow(selected >= 0 ? selected : 0);
    };
    filterBox_.onEscapeKey = [this] { filterBox_.clear(); };
    addAndMakeVisible(filterBox_);

    listBox_.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff181c22));
    listBox_.setRowHeight(24);
    addAndMakeVisible(listBox_);
    noMatches_.setColour(juce::Label::textColourId, juce::Colour(0xff8a94a3));
    noMatches_.setJustificationType(juce::Justification::centredTop);
    noMatches_.setInterceptsMouseClicks(false, false);
    addChildComponent(noMatches_);

    RebuildRows();
    listBox_.updateContent();
}

void NodePalette::SetDiagramType(std::string diagramType) {
    diagramType_ = std::move(diagramType);
    RefreshFromRegistry();
}

void NodePalette::RefreshFromRegistry() {
    entries_.clear();
    for (const auto& [typeName, descriptor] : registry_.Types()) {
        if (!ce::node_system::AllowedInDiagram(descriptor, diagramType_)) {
            continue; // belongs to another kind of graph
        }
        entries_.push_back({typeName, descriptor.displayName.empty() ? typeName : descriptor.displayName, descriptor.category,
                            descriptor.description});
    }
    std::sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) {
        return a.category != b.category ? a.category < b.category : a.displayName < b.displayName;
    });
    for (const auto& entry : entries_) {
        categoryExpanded_.emplace(entry.category, true);
    }

    RebuildRows();
    listBox_.updateContent();
    repaint();
}

namespace {
// How well an entry matches the search, lower is better; -1 if it does not. Every word must be found somewhere: the
// name (a word of it starting with the search word is best), the category, the type id or the description.
int MatchScore(const juce::StringArray& words, const juce::String& name, const juce::String& category, const juce::String& typeName,
               const juce::String& description) {
    int score = 0;
    juce::StringArray nameWords;
    nameWords.addTokens(name.toLowerCase(), " ()-_.", "");
    for (const auto& word : words) {
        bool startsNameWord = false;
        for (const auto& w : nameWords)
            startsNameWord = startsNameWord || w.startsWith(word);
        if (name.startsWithIgnoreCase(word)) score += 0;
        else if (startsNameWord) score += 1;
        else if (name.containsIgnoreCase(word)) score += 2;
        else if (category.containsIgnoreCase(word)) score += 4;
        else if (typeName.containsIgnoreCase(word)) score += 6;
        else if (description.containsIgnoreCase(word)) score += 8;
        else return -1;
    }
    return score;
}
} // namespace

void NodePalette::RebuildRows() {
    rows_.clear();
    const juce::String filter = filterBox_.getText().trim();

    // Searching: one list, best matches first, each with its category beside it.
    if (filter.isNotEmpty()) {
        juce::StringArray words;
        words.addTokens(filter.toLowerCase(), " ", "\"");
        words.removeEmptyStrings();
        std::vector<std::pair<int, std::size_t>> found;
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            const auto& e = entries_[i];
            const int score = MatchScore(words, juce::String(e.displayName), juce::String(e.category), juce::String(e.typeName),
                                         juce::String(e.description));
            if (score >= 0)
                found.push_back({ score, i });
        }
        std::stable_sort(found.begin(), found.end(), [this](const auto& a, const auto& b) {
            return a.first != b.first ? a.first < b.first : entries_[a.second].displayName < entries_[b.second].displayName;
        });
        for (const auto& [score, index] : found) {
            Row row;
            row.entryIndex = static_cast<int>(index);
            rows_.push_back(row);
        }
        noMatches_.setText("No nodes match \"" + filter + "\".", juce::dontSendNotification);
        noMatches_.setVisible(rows_.empty());
        return;
    }
    noMatches_.setVisible(false);

    // Browsing: by category, each section folding open and shut.
    std::size_t i = 0;
    while (i < entries_.size()) {
        const std::string category = entries_[i].category;
        std::vector<std::size_t> indices;
        std::size_t j = i;
        while (j < entries_.size() && entries_[j].category == category) {
            indices.push_back(j);
            ++j;
        }
        Row header;
        header.isHeader = true;
        header.category = category;
        header.count = static_cast<int>(indices.size());
        rows_.push_back(header);
        if (categoryExpanded_[category]) {
            for (const auto idx : indices) {
                Row row;
                row.entryIndex = static_cast<int>(idx);
                rows_.push_back(row);
            }
        }
        i = j;
    }
}

void NodePalette::AddRow(int row) {
    if (row < 0 || row >= static_cast<int>(rows_.size()) || rows_[static_cast<std::size_t>(row)].isHeader || !onAddRequested)
        return;
    const auto index = rows_[static_cast<std::size_t>(row)].entryIndex;
    if (index >= 0 && index < static_cast<int>(entries_.size()))
        onAddRequested(entries_[static_cast<std::size_t>(index)].typeName);
}

void NodePalette::listBoxItemDoubleClicked(int row, const juce::MouseEvent&) {
    AddRow(row);
}

void NodePalette::returnKeyPressed(int lastRowSelected) {
    AddRow(lastRowSelected);
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
    const bool searching = filterBox_.getText().trim().isNotEmpty();
    g.setColour(juce::Colour(0xffb8c4d5));
    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    g.drawText(entry.displayName, searching ? 8 : 20, 0, width - 20, height, juce::Justification::centredLeft, true);
    if (searching) {
        // Where it lives, for when you browse.
        g.setColour(juce::Colour(0xff6b7a8c));
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText(entry.category.empty() ? juce::String("Other") : juce::String(entry.category), 8, 0, width - 16, height,
                   juce::Justification::centredRight, true);
    }
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
    noMatches_.setBounds(bounds.reduced(8).removeFromTop(40));
}

void NodePalette::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff15181d));
}

} // namespace creation::node_editor_ui
