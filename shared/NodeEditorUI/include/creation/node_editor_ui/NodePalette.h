#pragma once

#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "node_system/type_registry.h"

namespace creation::node_editor_ui {

// Ported from apps/CreationEngine/Source/Views/NodeEditor/NodePalette.h - see
// NodeGraphComponent.h's own header comment for why this is a copy, not a shared-and-repointed
// move.
//
// A scrollable, draggable list of every type registered in the given registry -- drag a row onto
// NodeGraphComponent to create a node of that type. Relies on juce::ListBox's built-in drag
// support (ListBoxModel::getDragSourceDescription returning non-void triggers it automatically
// via the nearest ancestor juce::DragAndDropContainer).
//
// Pass a registry that ONLY contains the domain you want shown - this class has no per-domain
// filtering of its own, it lists everything in `registry.Types()`. Each domain (Signal Lab, video
// FX, Foley) is expected to own its own NodeTypeRegistry instance rather than sharing one, per
// the suite's explicit "share editing machinery, never merge catalogs" stance.
class NodePalette final : public juce::Component, private juce::ListBoxModel {
public:
    explicit NodePalette(const ce::node_system::NodeTypeRegistry& registry);

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    juce::var getDragSourceDescription(const juce::SparseSet<int>& selectedRows) override;

    // typeName is what itemDropped() actually needs to construct a node
    // (AddRegisteredNode dispatches on it); displayName/category are pure
    // presentation, falling back to typeName when a registration hasn't
    // set one (existing FRust node types, mid-migration to this field).
    struct Entry { std::string typeName, displayName, category; };
    std::vector<Entry> entries_;
    juce::Label titleLabel_{ {}, "Nodes" };
    juce::ListBox listBox_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodePalette)
};

} // namespace creation::node_editor_ui
