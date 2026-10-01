#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "node_system/graph.h"
#include "node_system/type_registry.h"

namespace creation::node_editor_ui
{

// The Variables panel: lists a graph's symbols - params, constants and variables (shared/NodeSystem/SYMBOLS.md) -
// and edits them. "+" adds one; selecting one shows its name, kind, type, value, access, persistent flag and
// description; dragging a row onto a NodeGraphComponent adds a Get node bound to it. The suite's standard panel for
// every node system - apps host it, they do not reimplement it.
class SymbolsPanel final : public juce::Component, private juce::ListBoxModel
{
public:
    explicit SymbolsPanel(ce::node_system::Graph& graph);
    ~SymbolsPanel() override;

    // A symbol was added, removed or changed: the owner marks its document edited and re-evaluates / recompiles.
    std::function<void()> onSymbolsChanged;

    // The data types this app's nodes can use (default: number, integer, toggle, colour, text).
    void setAllowedTypes(std::vector<ce::node_system::DataType> types);

    // The enums the app's nodes use (the registry's RegisterEnum list): each becomes a Choice type - a symbol whose
    // value is picked from a dropdown, and whose Get node wires into settings of that enum.
    void setEnums(const ce::node_system::NodeTypeRegistry& registry);

    // Re-reads the graph (after it was loaded or replaced).
    void refresh();

    // The graph's nodes changed (a Get node added, removed or rebound): updates each symbol's use count and the type
    // lock without rebuilding the editor, so typing is never interrupted. Apps call it from their graph-edited path.
    void graphChanged();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class Editor;

    // One entry of the type list: a plain data type, or a Choice of one enum.
    struct TypeChoice
    {
        ce::node_system::DataType type;
        std::string enumType;
        juce::String label;
    };
    std::vector<TypeChoice> typeChoices() const;
    const ce::node_system::EnumDef* enumOf(const ce::node_system::Symbol& symbol) const;
    juce::String typeLabel(const ce::node_system::Symbol& symbol) const;

    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
    void selectedRowsChanged(int row) override;
    juce::var getDragSourceDescription(const juce::SparseSet<int>& rows) override;

    void showAddMenu();
    void addSymbol(ce::node_system::SymbolKind kind, const TypeChoice& type);
    void changed();

    ce::node_system::Graph& graph;
    std::vector<ce::node_system::DataType> allowedTypes;
    const ce::node_system::NodeTypeRegistry* enumSource = nullptr;
    juce::Label title;
    juce::TextButton addButton { "+" };
    juce::ListBox list;
    std::unique_ptr<Editor> editor;
    juce::String selectedId;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SymbolsPanel)
};

// Display names used by the panel (and by apps that show symbols elsewhere).
juce::String symbolTypeName(ce::node_system::DataType type);
juce::String symbolKindName(ce::node_system::SymbolKind kind);

} // namespace creation::node_editor_ui
