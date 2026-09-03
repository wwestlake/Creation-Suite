#include <creation/node_editor_ui/NodeGraphComponent.h>

#include <algorithm>
#include <cmath>

namespace creation::node_editor_ui {

namespace {

using ce::node_system::Connection;
using ce::node_system::ConnectionId;
using ce::node_system::Node;
using ce::node_system::NodeId;
using ce::node_system::Pin;
using ce::node_system::PinId;
using ce::node_system::PinKind;

constexpr float kNodeWidth = 170.0f;
constexpr float kHeaderHeight = 24.0f;
constexpr float kRowHeight = 20.0f;
constexpr float kBottomPadding = 8.0f;
constexpr float kPinRadius = 5.0f;
constexpr float kPinHitPadding = 5.0f;
constexpr float kMinZoom = 0.35f;
constexpr float kMaxZoom = 2.5f;

juce::Colour HeaderColourFor(ce::node_system::Domain domain) {
    switch (domain) {
        case ce::node_system::Domain::Event: return juce::Colour(0xffb8722f);
        case ce::node_system::Domain::Core: return juce::Colour(0xff2f6fb8);
        case ce::node_system::Domain::Animation: return juce::Colour(0xff5f9c5a);
        case ce::node_system::Domain::Material: return juce::Colour(0xff9c5a9c);
        case ce::node_system::Domain::Audio: return juce::Colour(0xff9c8a5a);
    }
    return juce::Colour(0xff444444);
}

// One distinct, readable color per DataType -- previously every Data pin
// shared one fixed teal regardless of type, which made it impossible to
// tell at a glance whether two pins were even wireable to each other
// (found during the Pod plan's post-implementation verification pass).
// Exec pins stay white, unrelated to this table.
juce::Colour DataTypeColourFor(ce::node_system::DataType type) {
    using ce::node_system::DataType;
    switch (type) {
        case DataType::Any:           return juce::Colour(0xff9a9a9a);
        case DataType::Float:         return juce::Colour(0xff7fd0e8);
        case DataType::Vec2:          return juce::Colour(0xff6fb8e0);
        case DataType::Vec3:          return juce::Colour(0xff5a9ce0);
        case DataType::Vec4:          return juce::Colour(0xff4a80e0);
        case DataType::Color:         return juce::Colour(0xffe0b84a);
        case DataType::Bool:          return juce::Colour(0xffd85a5a);
        case DataType::Int:           return juce::Colour(0xff5ad8a0);
        case DataType::String:        return juce::Colour(0xffd85ad0);
        case DataType::Transform:     return juce::Colour(0xffe08a3a);
        case DataType::BoneTransform: return juce::Colour(0xffe0a83a);
        case DataType::Texture:       return juce::Colour(0xff9c5a9c);
        case DataType::AudioSignal:   return juce::Colour(0xff9c8a5a);
        case DataType::Entity:        return juce::Colour(0xff3ad8d8);
        case DataType::Function:      return juce::Colour(0xffb8b83a);
        case DataType::Material:      return juce::Colour(0xffc85ac8);
        case DataType::Model:         return juce::Colour(0xff5ac878);
        case DataType::Controller:    return juce::Colour(0xffc8785a);
    }
    return juce::Colour(0xff7fd0e8);
}

juce::Colour PinColourFor(const Pin& pin) {
    return pin.type.kind == PinKind::Exec ? juce::Colours::white : DataTypeColourFor(pin.type.dataType);
}

} // namespace

NodeGraphComponent::NodeGraphComponent(ce::node_system::Graph& graph, const ce::node_system::NodeTypeRegistry& registry)
    : graph_(graph), registry_(registry) {
    setWantsKeyboardFocus(true);
}

void NodeGraphComponent::ClearSelection() {
    SelectNode(0);
}

void NodeGraphComponent::GraphReplaced() {
    selectedNode_ = 0;
    errorNode_ = 0;
    selectedConnection_ = 0;
    draggingNode_ = false;
    draggingWire_ = false;
    panning_ = false;
    if (onSelectionChanged) {
        onSelectionChanged(0);
    }
    repaint();
}

juce::Point<float> NodeGraphComponent::WorldToScreen(juce::Point<float> world) const {
    return { world.x * zoom_ + viewOffset_.x, world.y * zoom_ + viewOffset_.y };
}

juce::Point<float> NodeGraphComponent::ScreenToWorld(juce::Point<float> screen) const {
    return { (screen.x - viewOffset_.x) / zoom_, (screen.y - viewOffset_.y) / zoom_ };
}

float NodeGraphComponent::NodeWorldHeight(const Node& node) const {
    const std::size_t rows = std::max(node.Inputs().size(), node.Outputs().size());
    return kHeaderHeight + static_cast<float>(rows) * kRowHeight + kBottomPadding;
}

juce::Rectangle<float> NodeGraphComponent::NodeScreenBounds(const Node& node) const {
    const auto topLeft = WorldToScreen({ node.EditorX(), node.EditorY() });
    return { topLeft.x, topLeft.y, kNodeWidth * zoom_, NodeWorldHeight(node) * zoom_ };
}

juce::Point<float> NodeGraphComponent::PinScreenPos(const Node& node, const Pin& pin, std::size_t rowIndex) const {
    const float localX = pin.isInput ? 0.0f : kNodeWidth;
    const float localY = kHeaderHeight + (static_cast<float>(rowIndex) + 0.5f) * kRowHeight;
    return WorldToScreen({ node.EditorX() + localX, node.EditorY() + localY });
}

NodeId NodeGraphComponent::HitTestNode(juce::Point<float> screenPos) const {
    for (const auto& [id, node] : graph_.Nodes()) {
        if (NodeScreenBounds(*node).contains(screenPos)) {
            return id;
        }
    }
    return 0;
}

bool NodeGraphComponent::HitTestPin(juce::Point<float> screenPos, PinHit& outHit) const {
    const float radius = kPinRadius * zoom_ + kPinHitPadding;
    for (const auto& [id, node] : graph_.Nodes()) {
        const auto& inputs = node->Inputs();
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            if (PinScreenPos(*node, inputs[i], i).getDistanceFrom(screenPos) <= radius) {
                outHit = { id, inputs[i].id, true };
                return true;
            }
        }
        const auto& outputs = node->Outputs();
        for (std::size_t i = 0; i < outputs.size(); ++i) {
            if (PinScreenPos(*node, outputs[i], i).getDistanceFrom(screenPos) <= radius) {
                outHit = { id, outputs[i].id, false };
                return true;
            }
        }
    }
    return false;
}

bool NodeGraphComponent::ConnectionScreenEndpoints(const Connection& conn, juce::Point<float>& outFrom,
                                                    juce::Point<float>& outTo) const {
    const Node* fromNode = graph_.FindNode(conn.fromNode);
    const Node* toNode = graph_.FindNode(conn.toNode);
    if (fromNode == nullptr || toNode == nullptr) return false;
    const Pin* fromPin = fromNode->FindPin(conn.fromPin);
    const Pin* toPin = toNode->FindPin(conn.toPin);
    if (fromPin == nullptr || toPin == nullptr) return false;
    const auto& fromOutputs = fromNode->Outputs();
    const auto& toInputs = toNode->Inputs();
    const auto fromIt = std::find_if(fromOutputs.begin(), fromOutputs.end(),
                                      [&](const Pin& p) { return p.id == fromPin->id; });
    const auto toIt = std::find_if(toInputs.begin(), toInputs.end(), [&](const Pin& p) { return p.id == toPin->id; });
    if (fromIt == fromOutputs.end() || toIt == toInputs.end()) return false;
    outFrom = PinScreenPos(*fromNode, *fromPin, static_cast<std::size_t>(fromIt - fromOutputs.begin()));
    outTo = PinScreenPos(*toNode, *toPin, static_cast<std::size_t>(toIt - toInputs.begin()));
    return true;
}

ce::node_system::ConnectionId NodeGraphComponent::HitTestConnection(juce::Point<float> screenPos) const {
    constexpr float kHitDistance = 6.0f;
    constexpr int kSamples = 24;
    ConnectionId best = 0;
    float bestDistance = kHitDistance;
    for (const Connection& conn : graph_.Connections()) {
        juce::Point<float> from, to;
        if (!ConnectionScreenEndpoints(conn, from, to)) continue;
        // Same cubic bezier DrawWire() actually draws -- sampled rather
        // than using a JUCE Path nearest-point query, so the hit-test
        // matches the drawn curve exactly with no extra API surface.
        const float dx = std::max(30.0f, std::abs(to.x - from.x) * 0.5f);
        const juce::Point<float> c1{ from.x + dx, from.y };
        const juce::Point<float> c2{ to.x - dx, to.y };
        for (int i = 0; i <= kSamples; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(kSamples);
            const float u = 1.0f - t;
            const float bx = u * u * u * from.x + 3.0f * u * u * t * c1.x + 3.0f * u * t * t * c2.x + t * t * t * to.x;
            const float by = u * u * u * from.y + 3.0f * u * u * t * c1.y + 3.0f * u * t * t * c2.y + t * t * t * to.y;
            const float distance = juce::Point<float>(bx, by).getDistanceFrom(screenPos);
            if (distance < bestDistance) {
                bestDistance = distance;
                best = conn.id;
            }
        }
    }
    return best;
}

void NodeGraphComponent::SelectNode(NodeId id) {
    // Node and connection selection are mutually exclusive -- selecting
    // either clears the other, same as any single-selection tool.
    selectedConnection_ = 0;
    if (selectedNode_ == id) {
        return;
    }
    selectedNode_ = id;
    if (onSelectionChanged) {
        onSelectionChanged(id);
    }
    repaint();
}

void NodeGraphComponent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff11151b));

    // Adaptive graph-paper grid, purely decorative -- helps make pan/zoom
    // legible across the whole zoom range. Previously a single fixed dot
    // spacing that just vanished entirely below a zoom threshold; now
    // minor lines fade out at low zoom (removing clutter/detail) while
    // major lines (every 4th minor line) stay visible across the entire
    // 0.35x-2.5x zoom range, so structure is never fully gone. Node/
    // Behavior Graph Foundations UX plan Phase 4.
    const float minorSpacing = 32.0f * zoom_;
    const float majorSpacing = minorSpacing * 4.0f;

    if (majorSpacing > 4.0f) {
        g.setColour(juce::Colour(0xff26313f));
        for (float x = std::fmod(viewOffset_.x, majorSpacing); x < static_cast<float>(getWidth()); x += majorSpacing)
            g.drawVerticalLine(static_cast<int>(x), 0.0f, static_cast<float>(getHeight()));
        for (float y = std::fmod(viewOffset_.y, majorSpacing); y < static_cast<float>(getHeight()); y += majorSpacing)
            g.drawHorizontalLine(static_cast<int>(y), 0.0f, static_cast<float>(getWidth()));
    }
    if (minorSpacing > 16.0f) {
        g.setColour(juce::Colour(0xff1a212a));
        for (float x = std::fmod(viewOffset_.x, minorSpacing); x < static_cast<float>(getWidth()); x += minorSpacing)
            g.drawVerticalLine(static_cast<int>(x), 0.0f, static_cast<float>(getHeight()));
        for (float y = std::fmod(viewOffset_.y, minorSpacing); y < static_cast<float>(getHeight()); y += minorSpacing)
            g.drawHorizontalLine(static_cast<int>(y), 0.0f, static_cast<float>(getWidth()));
    }

    for (const Connection& conn : graph_.Connections()) {
        juce::Point<float> fromPos, toPos;
        if (!ConnectionScreenEndpoints(conn, fromPos, toPos)) continue;
        if (conn.id == selectedConnection_) {
            // Selected wire drawn thicker/white, same visual language the
            // selected-node outline already uses -- Node/Behavior Graph
            // Foundations UX plan Phase 2.
            juce::Path path;
            path.startNewSubPath(fromPos);
            const float dx = std::max(30.0f, std::abs(toPos.x - fromPos.x) * 0.5f);
            path.cubicTo(fromPos.x + dx, fromPos.y, toPos.x - dx, toPos.y, toPos.x, toPos.y);
            g.setColour(juce::Colours::white);
            g.strokePath(path, juce::PathStrokeType(3.5f));
            continue;
        }
        const Node* fromNode = graph_.FindNode(conn.fromNode);
        const Pin* fromPin = fromNode ? fromNode->FindPin(conn.fromPin) : nullptr;
        DrawWire(g, fromPos, toPos, fromPin ? PinColourFor(*fromPin) : juce::Colours::grey);
    }

    if (draggingWire_) {
        const Node* startNode = graph_.FindNode(wireStart_.nodeId);
        if (startNode != nullptr) {
            const Pin* startPin = startNode->FindPin(wireStart_.pinId);
            if (startPin != nullptr) {
                const auto& pins = wireStart_.isInput ? startNode->Inputs() : startNode->Outputs();
                const auto it = std::find_if(pins.begin(), pins.end(), [&](const Pin& p) { return p.id == startPin->id; });
                if (it != pins.end()) {
                    const auto startPos = PinScreenPos(*startNode, *startPin, static_cast<std::size_t>(it - pins.begin()));
                    DrawWire(g, wireStart_.isInput ? wireDragScreenPos_ : startPos,
                              wireStart_.isInput ? startPos : wireDragScreenPos_, juce::Colours::white);
                }
            }
        }
    }

    std::vector<NodeId> nodeIds;
    nodeIds.reserve(graph_.Nodes().size());
    for (const auto& [id, node] : graph_.Nodes()) {
        nodeIds.push_back(id);
    }
    std::sort(nodeIds.begin(), nodeIds.end());
    for (NodeId id : nodeIds) {
        DrawNode(g, *graph_.FindNode(id));
    }
}

void NodeGraphComponent::DrawWire(juce::Graphics& g, juce::Point<float> from, juce::Point<float> to, juce::Colour colour) {
    juce::Path path;
    path.startNewSubPath(from);
    const float dx = std::max(30.0f, std::abs(to.x - from.x) * 0.5f);
    path.cubicTo(from.x + dx, from.y, to.x - dx, to.y, to.x, to.y);
    g.setColour(colour);
    g.strokePath(path, juce::PathStrokeType(2.0f));
}

void NodeGraphComponent::DrawNode(juce::Graphics& g, const Node& node) {
    const auto bounds = NodeScreenBounds(node);
    const bool selected = node.Id() == selectedNode_;

    g.setColour(juce::Colour(0xff1d2530));
    g.fillRoundedRectangle(bounds, 6.0f);

    auto headerBounds = bounds.withHeight(kHeaderHeight * zoom_);
    g.setColour(HeaderColourFor(node.NodeDomain()));
    g.fillRoundedRectangle(headerBounds, 6.0f);
    g.fillRect(headerBounds.withTop(headerBounds.getBottom() - 6.0f * zoom_)); // square off the rounded bottom corners.

    const auto* descriptor = registry_.Find(node.TypeName());
    const std::string& title = (descriptor != nullptr && !descriptor->displayName.empty()) ? descriptor->displayName : node.TypeName();
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(std::max(10.0f, 13.0f * zoom_))).boldened());
    g.drawText(title, headerBounds.reduced(6.0f * zoom_, 0.0f), juce::Justification::centredLeft, true);

    g.setColour(selected ? juce::Colours::white : juce::Colour(0xff384354));
    g.drawRoundedRectangle(bounds, 6.0f, selected ? 2.0f : 1.0f);

    // Drawn ADDITIONALLY (a slightly larger, outer red ring), not instead of the selection
    // outline -- a node can be both selected AND the current diagnostic's target at once, and
    // both facts stay visible.
    if (node.Id() == errorNode_) {
        g.setColour(juce::Colours::red);
        g.drawRoundedRectangle(bounds.expanded(2.5f), 7.0f, 2.0f);
    }

    g.setFont(juce::Font(juce::FontOptions(std::max(9.0f, 11.0f * zoom_))));
    const auto& inputs = node.Inputs();
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        const auto pos = PinScreenPos(node, inputs[i], i);
        g.setColour(PinColourFor(inputs[i]));
        g.fillEllipse(pos.x - kPinRadius * zoom_, pos.y - kPinRadius * zoom_, kPinRadius * 2.0f * zoom_,
                       kPinRadius * 2.0f * zoom_);
        g.setColour(juce::Colours::lightgrey);
        g.drawText(inputs[i].name,
                   juce::Rectangle<float>(pos.x + 8.0f * zoom_, pos.y - kRowHeight * zoom_ * 0.5f,
                                           (kNodeWidth * 0.5f) * zoom_, kRowHeight * zoom_),
                   juce::Justification::centredLeft, true);
    }
    const auto& outputs = node.Outputs();
    for (std::size_t i = 0; i < outputs.size(); ++i) {
        const auto pos = PinScreenPos(node, outputs[i], i);
        g.setColour(PinColourFor(outputs[i]));
        g.fillEllipse(pos.x - kPinRadius * zoom_, pos.y - kPinRadius * zoom_, kPinRadius * 2.0f * zoom_,
                       kPinRadius * 2.0f * zoom_);
        g.setColour(juce::Colours::lightgrey);
        g.drawText(outputs[i].name,
                   juce::Rectangle<float>(pos.x - (kNodeWidth * 0.5f + 8.0f) * zoom_, pos.y - kRowHeight * zoom_ * 0.5f,
                                           (kNodeWidth * 0.5f) * zoom_, kRowHeight * zoom_),
                   juce::Justification::centredRight, true);
    }
}

void NodeGraphComponent::resized() {}

void NodeGraphComponent::mouseDown(const juce::MouseEvent& event) {
    grabKeyboardFocus();
    const auto screenPos = event.position;

    PinHit hit;
    if (HitTestPin(screenPos, hit)) {
        draggingWire_ = true;
        wireStart_ = hit;
        wireDragScreenPos_ = screenPos;
        repaint();
        return;
    }

    const NodeId hitNode = HitTestNode(screenPos);
    if (hitNode != 0) {
        SelectNode(hitNode);
        draggingNode_ = true;
        dragNodeId_ = hitNode;
        dragStartMouseWorld_ = ScreenToWorld(screenPos);
        const Node* node = graph_.FindNode(hitNode);
        dragStartNodeWorld_ = { node->EditorX(), node->EditorY() };
        return;
    }

    // Checked after nodes/pins so a node overlapping a wire's path still
    // takes priority -- clicking a wire only counts when nothing else
    // was hit. Node/Behavior Graph Foundations UX plan Phase 2.
    const ConnectionId hitConnection = HitTestConnection(screenPos);
    if (hitConnection != 0) {
        SelectNode(0);
        selectedConnection_ = hitConnection;
        repaint();
        return;
    }

    SelectNode(0);
    panning_ = true;
    panStartMouse_ = screenPos;
    panStartOffset_ = viewOffset_;
}

void NodeGraphComponent::mouseDrag(const juce::MouseEvent& event) {
    if (draggingWire_) {
        wireDragScreenPos_ = event.position;
        repaint();
        return;
    }
    if (draggingNode_) {
        Node* node = graph_.FindNode(dragNodeId_);
        if (node != nullptr) {
            const auto worldNow = ScreenToWorld(event.position);
            const auto delta = worldNow - dragStartMouseWorld_;
            node->SetEditorPosition(dragStartNodeWorld_.x + delta.x, dragStartNodeWorld_.y + delta.y);
            repaint();
        }
        return;
    }
    if (panning_) {
        viewOffset_ = panStartOffset_ + (event.position - panStartMouse_);
        repaint();
    }
}

void NodeGraphComponent::mouseUp(const juce::MouseEvent& event) {
    if (draggingWire_) {
        draggingWire_ = false;
        PinHit target;
        if (HitTestPin(event.position, target) && target.nodeId != 0) {
            // Normalize to (output -> input) regardless of which end the drag started from -- a
            // user can just as naturally drag from an input backwards to an output.
            PinHit outputEnd = wireStart_.isInput ? target : wireStart_;
            PinHit inputEnd = wireStart_.isInput ? wireStart_ : target;
            if (!outputEnd.isInput && inputEnd.isInput && !(outputEnd.nodeId == inputEnd.nodeId && outputEnd.pinId == inputEnd.pinId)) {
                // An input accepts at most one incoming wire (the codegen walk assumes this) --
                // dragging a new wire onto an already-connected input replaces the old one, the
                // expected UX for "rewiring."
                std::vector<ce::node_system::ConnectionId> toRemove;
                for (const Connection& conn : graph_.Connections()) {
                    if (conn.toNode == inputEnd.nodeId && conn.toPin == inputEnd.pinId) {
                        toRemove.push_back(conn.id);
                    }
                }
                for (auto id : toRemove) {
                    graph_.Disconnect(id);
                }
                ce::node_system::ConnectError error{};
                const auto result = graph_.Connect(outputEnd.nodeId, outputEnd.pinId, inputEnd.nodeId, inputEnd.pinId, &error);
                if (result && onGraphChanged) {
                    onGraphChanged();
                }
            }
        }
        repaint();
        return;
    }
    draggingNode_ = false;
    panning_ = false;
}

void NodeGraphComponent::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) {
    const auto worldBefore = ScreenToWorld(event.position);
    const float factor = wheel.deltaY > 0.0f ? 1.1f : (1.0f / 1.1f);
    zoom_ = std::clamp(zoom_ * factor, kMinZoom, kMaxZoom);
    const auto screenAfter = WorldToScreen(worldBefore);
    viewOffset_ += event.position - screenAfter;
    repaint();
}

bool NodeGraphComponent::keyPressed(const juce::KeyPress& key) {
    if (key != juce::KeyPress::deleteKey && key != juce::KeyPress::backspaceKey) {
        return false;
    }
    if (selectedConnection_ != 0) {
        // Node/Behavior Graph Foundations UX plan Phase 2 -- previously
        // the only way to remove a connection was deleting one of the
        // two nodes it touched.
        graph_.Disconnect(selectedConnection_);
        selectedConnection_ = 0;
        if (onGraphChanged) {
            onGraphChanged();
        }
        repaint();
        return true;
    }
    if (selectedNode_ != 0) {
        graph_.RemoveNode(selectedNode_);
        selectedNode_ = 0;
        if (onSelectionChanged) {
            onSelectionChanged(0);
        }
        if (onGraphChanged) {
            onGraphChanged();
        }
        repaint();
        return true;
    }
    return false;
}

bool NodeGraphComponent::isInterestedInDragSource(const SourceDetails& details) {
    return details.description.isString();
}

void NodeGraphComponent::itemDropped(const SourceDetails& details) {
    const auto typeName = details.description.toString().toStdString();
    std::string error;
    Node* node = ce::node_system::AddRegisteredNode(graph_, registry_, typeName, &error);
    if (node == nullptr) {
        return;
    }
    const auto world = ScreenToWorld(details.localPosition.toFloat());
    node->SetEditorPosition(world.x, world.y);
    SelectNode(node->Id());
    if (onGraphChanged) {
        onGraphChanged();
    }
    repaint();
}

} // namespace creation::node_editor_ui
