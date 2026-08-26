#pragma once

#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "node_system/graph.h"

namespace creation::node_editor_ui {

// Ported from apps/CreationEngine/Source/Views/NodeEditor/NodeInspector.h - see
// NodeGraphComponent.h's own header comment for why this is a copy, not a shared-and-repointed
// move.
//
// Edits the selected node's UNCONNECTED Data input pins -- the per-instance "configuration"
// convention node catalogs rely on (a Log node's literal message, a GetVariable's variable name,
// an ordinary Compare node's threshold). A connected input shows "(connected)" instead of an
// editable field, since its default is never consulted by codegen once wired. Exec pins and
// Entity-typed pins (no CEL literal exists) are never editable.
class NodeInspector final : public juce::Component {
public:
    explicit NodeInspector(ce::node_system::Graph& graph);

    void SetSelectedNode(ce::node_system::NodeId nodeId);

    // Fired whenever a field commits a new value into a Pin's defaultValue -- the owning panel
    // uses this to regenerate a code preview/error highlight, the same way
    // NodeGraphComponent::onGraphChanged does for wiring/add/remove.
    std::function<void()> onValueChanged;

    void resized() override;
    void paint(juce::Graphics& g) override;

    static constexpr int kRowHeight = 22;

private:
    void RebuildRows();
    void AddUneditableRow(const juce::String& label);
    void AddFloatRow(const juce::String& label, ce::node_system::PinId pinId, bool isVec3Component, int vec3Index);
    void AddIntRow(const juce::String& label, ce::node_system::PinId pinId);
    void AddBoolRow(const juce::String& label, ce::node_system::PinId pinId);
    void AddStringRow(const juce::String& label, ce::node_system::PinId pinId);

    ce::node_system::Graph& graph_;
    ce::node_system::NodeId selectedNode_ = 0;

    juce::Label titleLabel_{ {}, "Inspector" };
    juce::Label noSelectionLabel_{ {}, "No node selected" };
    juce::Label nodeTypeLabel_;

    struct Row {
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Component> editor; // null for an uneditable informational row.
    };
    std::vector<Row> rows_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeInspector)
};

} // namespace creation::node_editor_ui
