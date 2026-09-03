#include <creation/node_editor_ui/NodeInspector.h>

#include <algorithm>

namespace creation::node_editor_ui {

namespace {

using ce::node_system::DataType;
using ce::node_system::Node;
using ce::node_system::NodeId;
using ce::node_system::Pin;
using ce::node_system::PinId;
using ce::node_system::PinKind;
using ce::node_system::Vec3Default;

Vec3Default ReadVec3(const Pin& pin) {
    if (const auto* v = std::get_if<Vec3Default>(&pin.defaultValue)) {
        return *v;
    }
    return {};
}

bool IsConnectedInput(const ce::node_system::Graph& graph, NodeId nodeId, PinId pinId) {
    for (const auto& conn : graph.Connections()) {
        if (conn.toNode == nodeId && conn.toPin == pinId) {
            return true;
        }
    }
    return false;
}

// Short, human-readable name for a row's tooltip -- the concrete
// complaint this answers: a user could see a labeled field but had no
// way to tell what type of value it actually held. Content-clarity
// pass, Node/Behavior Graph Foundations plan Phase 7.
juce::String DataTypeName(DataType type) {
    switch (type) {
        case DataType::Float: return "Float";
        case DataType::Vec2: return "Vec2";
        case DataType::Vec3: return "Vec3";
        case DataType::Vec4: return "Vec4";
        case DataType::Color: return "Color";
        case DataType::Bool: return "Bool";
        case DataType::Int: return "Int";
        case DataType::String: return "String";
        case DataType::Transform: return "Transform";
        case DataType::BoneTransform: return "Bone Transform";
        case DataType::Texture: return "Texture";
        case DataType::AudioSignal: return "Audio Signal";
        case DataType::Entity: return "Entity";
        case DataType::Function: return "Function";
        case DataType::Material: return "Material";
        case DataType::Model: return "Model";
        case DataType::Controller: return "Controller";
        default: return "Any";
    }
}

} // namespace

NodeInspector::NodeInspector(ce::node_system::Graph& graph, const ce::node_system::NodeTypeRegistry* typeRegistry)
    : graph_(graph), typeRegistry_(typeRegistry) {
    titleLabel_.setFont(juce::Font(juce::FontOptions(15.0f)).boldened());
    titleLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel_);

    noSelectionLabel_.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(noSelectionLabel_);

    nodeTypeLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    nodeTypeLabel_.setFont(juce::Font(juce::FontOptions(13.0f)).boldened());
    addAndMakeVisible(nodeTypeLabel_);

    nodeDescriptionLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff8a95a5));
    nodeDescriptionLabel_.setFont(juce::Font(juce::FontOptions(11.0f)));
    nodeDescriptionLabel_.setMinimumHorizontalScale(1.0f);
    addAndMakeVisible(nodeDescriptionLabel_);

    RebuildRows();
}

void NodeInspector::SetSelectedNode(ce::node_system::NodeId nodeId) {
    selectedNode_ = nodeId;
    RebuildRows();
    resized();
}

void NodeInspector::AddUneditableRow(const juce::String& text) {
    Row row;
    row.label = std::make_unique<juce::Label>(juce::String{}, text);
    row.label->setColour(juce::Label::textColourId, juce::Colour(0xff8a95a5));
    row.label->setFont(juce::Font(juce::FontOptions(12.0f)));
    addAndMakeVisible(*row.label);
    rows_.push_back(std::move(row));
}

void NodeInspector::AddFloatRow(const juce::String& label, PinId pinId, bool isVec3Component, int vec3Index) {
    Row row;
    row.label = std::make_unique<juce::Label>(juce::String{}, label);
    row.label->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    row.label->setFont(juce::Font(juce::FontOptions(12.0f)));
    addAndMakeVisible(*row.label);

    auto editor = std::make_unique<juce::TextEditor>();
    Node* node = graph_.FindNode(selectedNode_);
    const Pin* pin = node != nullptr ? node->FindPin(pinId) : nullptr;
    editor->setTooltip(isVec3Component ? "Vec3/Color component (Float)" : "Float value");
    if (pin != nullptr) {
        if (isVec3Component) {
            const Vec3Default v = ReadVec3(*pin);
            const float component = vec3Index == 0 ? v.x : (vec3Index == 1 ? v.y : v.z);
            editor->setText(juce::String(component), juce::dontSendNotification);
        } else if (const auto* f = std::get_if<float>(&pin->defaultValue)) {
            editor->setText(juce::String(*f), juce::dontSendNotification);
        } else {
            editor->setText("0.0", juce::dontSendNotification);
        }
    }

    NodeId nodeId = selectedNode_;
    auto* rawEditor = editor.get();
    auto commit = [this, nodeId, pinId, isVec3Component, vec3Index, rawEditor] {
        Node* n = graph_.FindNode(nodeId);
        Pin* p = n != nullptr ? n->FindPin(pinId) : nullptr;
        if (p == nullptr) {
            return;
        }
        const float value = rawEditor->getText().getFloatValue();
        if (isVec3Component) {
            Vec3Default v = ReadVec3(*p);
            if (vec3Index == 0) v.x = value;
            else if (vec3Index == 1) v.y = value;
            else v.z = value;
            p->defaultValue = v;
        } else {
            p->defaultValue = value;
        }
        if (onValueChanged) {
            onValueChanged();
        }
    };
    editor->onReturnKey = commit;
    editor->onFocusLost = commit;
    addAndMakeVisible(*editor);
    row.editor = std::move(editor);
    rows_.push_back(std::move(row));
}

void NodeInspector::AddIntRow(const juce::String& label, PinId pinId) {
    Row row;
    row.label = std::make_unique<juce::Label>(juce::String{}, label);
    row.label->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    row.label->setFont(juce::Font(juce::FontOptions(12.0f)));
    addAndMakeVisible(*row.label);

    auto editor = std::make_unique<juce::TextEditor>();
    Node* node = graph_.FindNode(selectedNode_);
    const Pin* pin = node != nullptr ? node->FindPin(pinId) : nullptr;
    editor->setTooltip("Int value");
    if (pin != nullptr) {
        if (const auto* i = std::get_if<std::int64_t>(&pin->defaultValue)) {
            editor->setText(juce::String(*i), juce::dontSendNotification);
        } else {
            editor->setText("0", juce::dontSendNotification);
        }
    }

    NodeId nodeId = selectedNode_;
    auto* rawEditor = editor.get();
    auto commit = [this, nodeId, pinId, rawEditor] {
        Node* n = graph_.FindNode(nodeId);
        Pin* p = n != nullptr ? n->FindPin(pinId) : nullptr;
        if (p != nullptr) {
            p->defaultValue = static_cast<std::int64_t>(rawEditor->getText().getLargeIntValue());
        }
        if (onValueChanged) {
            onValueChanged();
        }
    };
    editor->onReturnKey = commit;
    editor->onFocusLost = commit;
    addAndMakeVisible(*editor);
    row.editor = std::move(editor);
    rows_.push_back(std::move(row));
}

void NodeInspector::AddBoolRow(const juce::String& label, PinId pinId) {
    Row row;
    row.label = std::make_unique<juce::Label>(juce::String{}, label);
    row.label->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    row.label->setFont(juce::Font(juce::FontOptions(12.0f)));
    addAndMakeVisible(*row.label);

    auto editor = std::make_unique<juce::ToggleButton>();
    Node* node = graph_.FindNode(selectedNode_);
    const Pin* pin = node != nullptr ? node->FindPin(pinId) : nullptr;
    editor->setTooltip("Bool value");
    bool initial = false;
    if (pin != nullptr) {
        if (const auto* b = std::get_if<bool>(&pin->defaultValue)) {
            initial = *b;
        }
    }
    editor->setToggleState(initial, juce::dontSendNotification);

    NodeId nodeId = selectedNode_;
    auto* rawEditor = editor.get();
    editor->onClick = [this, nodeId, pinId, rawEditor] {
        Node* n = graph_.FindNode(nodeId);
        Pin* p = n != nullptr ? n->FindPin(pinId) : nullptr;
        if (p != nullptr) {
            p->defaultValue = rawEditor->getToggleState();
        }
        if (onValueChanged) {
            onValueChanged();
        }
    };
    addAndMakeVisible(*editor);
    row.editor = std::move(editor);
    rows_.push_back(std::move(row));
}

void NodeInspector::AddStringRow(const juce::String& label, PinId pinId) {
    Row row;
    row.label = std::make_unique<juce::Label>(juce::String{}, label);
    row.label->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    row.label->setFont(juce::Font(juce::FontOptions(12.0f)));
    addAndMakeVisible(*row.label);

    auto editor = std::make_unique<juce::TextEditor>();
    Node* node = graph_.FindNode(selectedNode_);
    const Pin* pin = node != nullptr ? node->FindPin(pinId) : nullptr;
    editor->setTooltip(pin != nullptr && pin->type.dataType == DataType::Texture
                            ? "Texture file path" : "String value");
    if (pin != nullptr) {
        if (const auto* s = std::get_if<std::string>(&pin->defaultValue)) {
            editor->setText(*s, juce::dontSendNotification);
        }
    }

    NodeId nodeId = selectedNode_;
    auto* rawEditor = editor.get();
    auto commit = [this, nodeId, pinId, rawEditor] {
        Node* n = graph_.FindNode(nodeId);
        Pin* p = n != nullptr ? n->FindPin(pinId) : nullptr;
        if (p != nullptr) {
            p->defaultValue = rawEditor->getText().toStdString();
        }
        if (onValueChanged) {
            onValueChanged();
        }
    };
    editor->onReturnKey = commit;
    editor->onFocusLost = commit;
    addAndMakeVisible(*editor);
    row.editor = std::move(editor);
    rows_.push_back(std::move(row));
}

void NodeInspector::RebuildRows() {
    rows_.clear(); // destroys owned Label/editor Components, auto-detaching from this parent.

    Node* node = graph_.FindNode(selectedNode_);
    if (node == nullptr) {
        nodeTypeLabel_.setText({}, juce::dontSendNotification);
        nodeDescriptionLabel_.setText({}, juce::dontSendNotification);
        return;
    }

    // Prefer the registered displayName/description over the raw dotted
    // typeName (e.g. "core.variable.get.bool") -- most registered types
    // already set these, this just surfaces them. Falls back to the
    // typeName itself when no registry was given or the type isn't
    // found, so this never regresses to showing nothing.
    const auto* descriptor = typeRegistry_ != nullptr ? typeRegistry_->Find(node->TypeName()) : nullptr;
    const bool hasDisplayName = descriptor != nullptr && !descriptor->displayName.empty();
    nodeTypeLabel_.setText(hasDisplayName ? descriptor->displayName : juce::String(node->TypeName()), juce::dontSendNotification);
    nodeTypeLabel_.setTooltip(node->TypeName());
    nodeDescriptionLabel_.setText(descriptor != nullptr ? juce::String(descriptor->description) : juce::String{},
                                   juce::dontSendNotification);

    for (const Pin& pin : node->Inputs()) {
        if (pin.type.kind == PinKind::Exec) {
            continue; // nothing to configure -- wiring only.
        }
        if (IsConnectedInput(graph_, selectedNode_, pin.id)) {
            AddUneditableRow(pin.name + " (connected)");
            continue;
        }
        switch (pin.type.dataType) {
            case DataType::Float:
                AddFloatRow(pin.name, pin.id, false, 0);
                break;
            case DataType::Int:
                AddIntRow(pin.name, pin.id);
                break;
            case DataType::Bool:
                AddBoolRow(pin.name, pin.id);
                break;
            case DataType::String:
            case DataType::Texture: // Same std::string backing (pin.h) as an
                                     // absolute file path -- no asset picker
                                     // yet, see material_nodes.cpp's Texture
                                     // Sample description.
                AddStringRow(pin.name, pin.id);
                break;
            case DataType::Vec3:
            case DataType::Color: // Same Vec3Default backing (see pin.h) -- an RGB triplet editor
                                   // is correct for both; a dedicated colour-picker UI is a
                                   // possible future upgrade, not a different data path.
                AddFloatRow(pin.name + ".x", pin.id, true, 0);
                AddFloatRow(pin.name + ".y", pin.id, true, 1);
                AddFloatRow(pin.name + ".z", pin.id, true, 2);
                break;
            case DataType::Entity:
                AddUneditableRow(pin.name + " (must be connected -- no entity literal)");
                break;
            default:
                AddUneditableRow(pin.name + " (unsupported type)");
                break;
        }
    }
}

void NodeInspector::resized() {
    auto bounds = getLocalBounds();
    titleLabel_.setBounds(bounds.removeFromTop(24));

    Node* node = graph_.FindNode(selectedNode_);
    noSelectionLabel_.setVisible(node == nullptr);
    nodeTypeLabel_.setVisible(node != nullptr);
    nodeDescriptionLabel_.setVisible(node != nullptr && nodeDescriptionLabel_.getText().isNotEmpty());
    if (node == nullptr) {
        noSelectionLabel_.setBounds(bounds.removeFromTop(18));
        return;
    }
    nodeTypeLabel_.setBounds(bounds.removeFromTop(20));
    if (nodeDescriptionLabel_.isVisible()) nodeDescriptionLabel_.setBounds(bounds.removeFromTop(28));
    bounds.removeFromTop(4);

    for (auto& row : rows_) {
        auto rowBounds = bounds.removeFromTop(kRowHeight);
        if (row.editor != nullptr) {
            row.label->setBounds(rowBounds.removeFromLeft(90));
            row.editor->setBounds(rowBounds.reduced(2, 1));
        } else {
            row.label->setBounds(rowBounds);
        }
    }
}

void NodeInspector::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff15181d));
}

} // namespace creation::node_editor_ui
