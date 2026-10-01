#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "node_system/graph.h"
#include "node_system/type_registry.h"

#include <functional>
#include <memory>
#include <vector>

namespace creation::node_editor_ui
{

// The Types panel (shared/NodeSystem/TYPES.md): the types in scope for a graph, and the editors that make them.
// Making a type is not part of any graph and not a node: a type is defined here and from then on it is one of the
// types things can be - a param's type, a setting's, what a Switch selector carries - wherever it is in scope.
//
//   This graph - the graph's own types, saved with it.
//   Project    - the project's types, for every graph in the project in any app; the app saves them.
//   Built-in   - the app's and pods' types, read-only here.
//
// Selecting a type opens its editor below the list. Enums so far; structs follow.
class TypesPanel final : public juce::Component, private juce::ListBoxModel
{
public:
    TypesPanel(ce::node_system::Graph& graph, const ce::node_system::NodeTypeRegistry& registry);
    ~TypesPanel() override;

    // The project's own enums, edited here. onProjectTypesChanged hands them back for the app to save and to put in
    // its registry (NodeTypeRegistry::ReplaceEnums). Not editable without an open project.
    void setProjectTypes(std::vector<ce::node_system::EnumDef> enums, bool editable);
    std::function<void(const std::vector<ce::node_system::EnumDef>&)> onProjectTypesChanged;
    // The graph's own types changed (or a use of a type was updated): the owner marks the document edited and
    // refreshes what shows types (Variables, Properties).
    std::function<void()> onGraphTypesChanged;

    // Re-reads the graph (after it was loaded or replaced, or its types changed elsewhere).
    void refresh();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class EnumEditor;
    friend class EnumEditor;

    struct Row
    {
        bool header = false;
        ce::node_system::TypeScope scope = ce::node_system::TypeScope::builtin;
        std::string name;    // the type's identifier
        juce::String title;  // shown
    };

    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
    void selectedRowsChanged(int row) override;

    void showAddMenu();
    void addEnum(ce::node_system::TypeScope scope);
    void rebuildRows();
    void refreshSoon();

    // The enum being edited, wherever it lives; null if it is gone. Read-only for built-in ones.
    ce::node_system::EnumDef* editableEnum(ce::node_system::TypeScope scope, const std::string& name);
    const ce::node_system::EnumDef* findEnum(ce::node_system::TypeScope scope, const std::string& name) const;
    // An enum changed: the graph's own -> onGraphTypesChanged; the project's -> onProjectTypesChanged.
    void enumChanged(ce::node_system::TypeScope scope);
    // How many things in this graph use an enum (params of it, settings of it).
    int usesInGraph(const std::string& name) const;
    // Values moved or removed: what this graph stores as numbers follows (mapping[old] = new, -1 = removed).
    void remapValues(const std::string& name, const std::vector<int>& mapping);
    // The enum is gone: what used it in this graph becomes a plain integer.
    void forgetEnum(const std::string& name);
    void removeType(ce::node_system::TypeScope scope, const std::string& name);

    ce::node_system::Graph& graph;
    const ce::node_system::NodeTypeRegistry& registry;
    std::vector<ce::node_system::EnumDef> projectEnums;
    bool projectEditable = false;

    juce::Label title;
    juce::TextButton addButton { "+" };
    juce::ListBox list;
    std::vector<Row> rows;
    ce::node_system::TypeScope selectedScope = ce::node_system::TypeScope::builtin;
    std::string selectedName;
    juce::Viewport editorView;              // declared before the editor it shows, so it outlives it
    std::unique_ptr<EnumEditor> editor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TypesPanel)
};

} // namespace creation::node_editor_ui
