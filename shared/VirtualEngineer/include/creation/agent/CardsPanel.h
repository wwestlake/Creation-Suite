#pragma once

#include <creation/agent/VirtualEngineer.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace creation::agent
{
// Writing LiteSemRAG cards (shared/LiteSemRag/README.md): every card in scope for an app's Virtual Engineer - shipped
// (read-only), the suite's, this app's, the open project's - and an editor for one card. "Try a request" shows which
// cards a request would bring up, without sending anything, so a new card can be checked as it is written.
class CardsPanel final : public juce::Component, private juce::ListBoxModel
{
public:
    explicit CardsPanel(VirtualEngineer& engineer);
    ~CardsPanel() override;

    // Re-reads the cards (after the project changed, or cards changed elsewhere).
    void refresh();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct Row
    {
        creation::litesemrag::CardScope scope;
        creation::litesemrag::Card card;
    };
    class Editor;

    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
    void selectedRowsChanged(int row) override;

    void showNewMenu();
    void select(int row);
    void startNew(creation::litesemrag::CardScope scope);
    void save();
    void retireOrDelete(bool deleteIt);
    void tryRequest();
    void problem(const juce::String& text, bool good = false);
    creation::litesemrag::CardStore::Place placeFor(creation::litesemrag::CardScope scope) const;

    VirtualEngineer& engineer;
    std::vector<Row> rows;
    int selectedRow = -1;
    bool editingNew = false;
    creation::litesemrag::CardScope newScope = creation::litesemrag::CardScope::suite;
    juce::String editingOriginalId;

    juce::Label title, status;
    juce::TextButton newButton { "+ New card" }, saveButton { "Save" }, retireButton { "Retire" }, deleteButton { "Delete" };
    juce::ListBox list;
    std::unique_ptr<Editor> editor;
    juce::Viewport editorView;
    juce::Label tryLabel, tryResult;
    juce::TextEditor tryRequestText;
    juce::TextButton tryButton { "Try" };
};
} // namespace creation::agent
