#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "node_system/graph.h"
#include "node_system/type_registry.h"

namespace creation::node_editor_ui
{

// Ported from apps/CreationEngine/Source/Views/NodeEditor/NodeGraphComponent.h - that original is
// left untouched (this is a copy-and-adapt into shared/, not a move-and-repoint of CreationEngine
// itself, since this session has no verified CreationEngine build environment to check that
// repointing against). Genuinely domain-agnostic: depends only on JUCE + node_system, exactly the
// same shape that made the original reusable in the first place.
//
// The interactive 2D canvas -- pan/zoom, node selection/drag, wire drag-to-connect, delete, and
// accepting a dropped node type from NodePalette. Owns no state of its own beyond view (pan/zoom)
// and interaction (drag/selection) -- the actual document is `graph_`, a reference to the
// ce::node_system::Graph the owning panel holds, so swapping the whole Graph out (load) just
// works without this component needing to know about files at all.
class NodeGraphComponent final : public juce::Component, public juce::DragAndDropTarget {
public:
    NodeGraphComponent(ce::node_system::Graph& graph, const ce::node_system::NodeTypeRegistry& registry);

    // Fired on every selection change, including to "none" (id 0 -- a real NodeId is never 0).
    std::function<void(ce::node_system::NodeId)> onSelectionChanged;

    // Fired after any edit that changes the graph's shape or wiring (node added/removed/moved is
    // NOT included -- only add/remove/connect/disconnect, the things that actually change
    // generated source).
    std::function<void()> onGraphChanged;

    ce::node_system::NodeId SelectedNode() const { return selectedNode_; }
    void ClearSelection();

    // Highlights the node a codegen/compile diagnostic was attributed to (a red outline, drawn in
    // addition to -- not instead of -- the normal selection outline), or clears it.
    void SetErrorNode(ce::node_system::NodeId id) { errorNode_ = id; repaint(); }
    void ClearErrorNode() { SetErrorNode(0); }

    // Re-reads `graph_` from scratch (e.g. after the owning panel replaces it wholesale via
    // Load) -- resets selection, keeps the current pan/zoom so loading a graph doesn't disorient
    // the view.
    void GraphReplaced();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;

    // juce::DragAndDropTarget -- accepts a plain string node-type-name description dragged from
    // NodePalette.
    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDropped(const SourceDetails& details) override;

private:
    struct PinHit {
        ce::node_system::NodeId nodeId = 0;
        ce::node_system::PinId pinId = 0;
        bool isInput = false;
    };

    juce::Point<float> WorldToScreen(juce::Point<float> world) const;
    juce::Point<float> ScreenToWorld(juce::Point<float> screen) const;
    juce::Rectangle<float> NodeScreenBounds(const ce::node_system::Node& node) const;
    float NodeWorldHeight(const ce::node_system::Node& node) const;
    juce::Point<float> PinScreenPos(const ce::node_system::Node& node, const ce::node_system::Pin& pin,
                                     std::size_t rowIndex) const;

    ce::node_system::NodeId HitTestNode(juce::Point<float> screenPos) const;
    bool HitTestPin(juce::Point<float> screenPos, PinHit& outHit) const;
    // Node/Behavior Graph Foundations UX plan Phase 2 -- there was no way
    // to remove a single connection before this, only whole-node deletion
    // or drag-a-replacement-wire onto an input. Shared by paint() (which
    // already computed these same endpoints inline) and the new
    // connection hit-test/delete path, so both agree on exactly where a
    // wire is drawn.
    bool ConnectionScreenEndpoints(const ce::node_system::Connection& conn, juce::Point<float>& outFrom,
                                    juce::Point<float>& outTo) const;
    ce::node_system::ConnectionId HitTestConnection(juce::Point<float> screenPos) const;

    void SelectNode(ce::node_system::NodeId id);
    void DrawNode(juce::Graphics& g, const ce::node_system::Node& node);
    void DrawWire(juce::Graphics& g, juce::Point<float> from, juce::Point<float> to, juce::Colour colour);

    ce::node_system::Graph& graph_;
    const ce::node_system::NodeTypeRegistry& registry_;

    juce::Point<float> viewOffset_{ 40.0f, 40.0f };
    float zoom_ = 1.0f;

    ce::node_system::NodeId selectedNode_ = 0;
    ce::node_system::NodeId errorNode_ = 0;
    ce::node_system::ConnectionId selectedConnection_ = 0;

    bool panning_ = false;
    juce::Point<float> panStartMouse_;
    juce::Point<float> panStartOffset_;

    bool draggingNode_ = false;
    ce::node_system::NodeId dragNodeId_ = 0;
    juce::Point<float> dragStartMouseWorld_;
    juce::Point<float> dragStartNodeWorld_;

    bool draggingWire_ = false;
    PinHit wireStart_;
    juce::Point<float> wireDragScreenPos_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeGraphComponent)
};

} // namespace creation::node_editor_ui
