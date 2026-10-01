#include <creation/node_editor_ui/SymbolsPanel.h>
#include <creation/node_editor_ui/NodeGraphComponent.h>
#include <node_system/symbol_nodes.h>

namespace ns = ce::node_system;

namespace creation::node_editor_ui
{
namespace
{
const juce::Colour background { 0xff15181d };
const juce::Colour rowColour { 0xff1b232c };

juce::Colour kindColour(ns::SymbolKind kind)
{
    switch (kind)
    {
        case ns::SymbolKind::Param: return juce::Colour(0xff80bfff);
        case ns::SymbolKind::Constant: return juce::Colour(0xff9aa8ba);
        case ns::SymbolKind::Variable: return juce::Colour(0xfff5c15a);
    }
    return juce::Colours::grey;
}

juce::String valueSummary(const ns::PinDefaultValue& v)
{
    if (const auto* f = std::get_if<float>(&v)) return juce::String(*f, 3);
    if (const auto* i = std::get_if<std::int64_t>(&v)) return juce::String(*i);
    if (const auto* b = std::get_if<bool>(&v)) return *b ? "on" : "off";
    if (const auto* s = std::get_if<std::string>(&v)) return "\"" + juce::String(*s) + "\"";
    if (const auto* c = std::get_if<ns::Vec3Default>(&v))
        return juce::String(c->x, 2) + ", " + juce::String(c->y, 2) + ", " + juce::String(c->z, 2);
    return {};
}

int usesOf(const ns::Graph& graph, const std::string& id)
{
    int uses = 0;
    for (const auto& [nodeId, node] : graph.Nodes())
        if (const auto* symbol = ns::SymbolForGetNode(graph, *node))
            if (symbol->id == id)
                ++uses;
    return uses;
}
}

juce::String symbolTypeName(ns::DataType type)
{
    switch (type)
    {
        case ns::DataType::Float: return "Number";
        case ns::DataType::Int: return "Integer";
        case ns::DataType::Bool: return "Toggle";
        case ns::DataType::Color: return "Color";
        case ns::DataType::Vec3: return "Vector";
        case ns::DataType::String: return "Text";
        default: return "Value";
    }
}

juce::String symbolKindName(ns::SymbolKind kind)
{
    switch (kind)
    {
        case ns::SymbolKind::Param: return "Param";
        case ns::SymbolKind::Constant: return "Constant";
        case ns::SymbolKind::Variable: return "Variable";
    }
    return {};
}

//==============================================================================
// Edits the selected symbol. Rebuilt whenever the selection changes; every edit writes straight into the graph.
class SymbolsPanel::Editor final : public juce::Component
{
public:
    Editor(SymbolsPanel& p, const std::string& symbolId) : panel(p), id(symbolId)
    {
        auto* symbol = panel.graph.FindSymbol(id);
        if (symbol == nullptr)
            return;
        name.setText(symbol->name, juce::dontSendNotification);
        name.onReturnKey = name.onFocusLost = [this]() {
            if (auto* s = symbol_()) { s->name = name.getText().trim().toStdString(); panel.changed(); }
        };
        row("Name", name);

        for (int k = 0; k < 3; ++k)
            kind.addItem(symbolKindName(static_cast<ns::SymbolKind>(k)), k + 1);
        kind.setSelectedId(static_cast<int>(symbol->kind) + 1, juce::dontSendNotification);
        kind.onChange = [this]() {
            if (auto* s = symbol_()) { s->kind = static_cast<ns::SymbolKind>(kind.getSelectedId() - 1); panel.changed(); rebuildSoon(); }
        };
        row("Kind", kind);

        const auto choices = panel.typeChoices();
        for (size_t i = 0; i < choices.size(); ++i)
        {
            if (i == panel.allowedTypes.size())
                type.addSectionHeading("Choice");
            type.addItem(choices[i].label, static_cast<int>(i) + 1);
            if (choices[i].type == symbol->type && choices[i].enumType == symbol->enumType)
                type.setSelectedId(static_cast<int>(i) + 1, juce::dontSendNotification);
        }
        type.onChange = [this]() {
            auto* s = symbol_();
            const auto all = panel.typeChoices();
            const int index = type.getSelectedId() - 1;
            if (s == nullptr || ! juce::isPositiveAndBelow(index, static_cast<int>(all.size())))
                return;
            s->type = all[static_cast<size_t>(index)].type;
            s->enumType = all[static_cast<size_t>(index)].enumType;
            s->value = ns::DefaultValueFor(s->type);
            panel.changed();
            rebuildSoon();
        };
        row("Type", type);

        buildValueEditor(*symbol);

        for (const auto& a : ns::SymbolAccessibilities())
            access.addItem(a, access.getNumItems() + 1);
        access.setText(symbol->accessibility, juce::dontSendNotification);
        access.setTooltip("Who can see or use it: private, graph, module, project, public, agent (the AI may use it), readonly, hidden.");
        access.onChange = [this]() {
            if (auto* s = symbol_()) { s->accessibility = access.getText().toStdString(); panel.changed(); }
        };
        row("Access", access);

        if (symbol->kind == ns::SymbolKind::Variable)
        {
            persistent.setButtonText("Kept between runs");
            persistent.setToggleState(symbol->persistent, juce::dontSendNotification);
            persistent.onClick = [this]() {
                if (auto* s = symbol_()) { s->persistent = persistent.getToggleState(); panel.changed(); }
            };
            row("Persistent", persistent);
        }

        description.setMultiLine(true);
        description.setReturnKeyStartsNewLine(false);
        description.setText(symbol->description, juce::dontSendNotification);
        description.onReturnKey = description.onFocusLost = [this]() {
            if (auto* s = symbol_()) { s->description = description.getText().toStdString(); panel.changed(); }
        };
        row("Description", description, 44);

        usesLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
        addAndMakeVisible(usesLabel);
        updateUses();

        remove.onClick = [this]() {
            panel.graph.RemoveSymbol(id);
            panel.selectedId.clear();
            panel.changed();
            panel.refresh();
        };
        addAndMakeVisible(remove);
    }

    int preferredHeight() const { return y + 70; }

    // How many Get nodes read this symbol. Changing the type would break their wires, so it is locked while any do.
    void updateUses()
    {
        const int uses = usesOf(panel.graph, id);
        type.setEnabled(uses == 0);
        type.setTooltip(uses == 0 ? juce::String() : "Used by " + juce::String(uses) + " node(s) - remove those Get nodes to change the type.");
        usesLabel.setText(uses == 0 ? "Not used yet - drag it onto the graph." : "Used by " + juce::String(uses) + " node(s).",
                          juce::dontSendNotification);
    }

    void resized() override
    {
        for (auto& [label, component, height, top] : rows)
        {
            label->setBounds(0, top, 84, 24);
            component->setBounds(88, top, getWidth() - 88, height);
        }
        usesLabel.setBounds(0, y + 4, getWidth(), 20);
        remove.setBounds(getWidth() - 90, y + 30, 90, 26);
    }

private:
    ns::Symbol* symbol_() { return panel.graph.FindSymbol(id); }

    void rebuildSoon()
    {
        juce::MessageManager::callAsync([safe = juce::Component::SafePointer<SymbolsPanel>(&panel)]() {
            if (safe != nullptr) safe->refresh();
        });
    }

    void row(const juce::String& text, juce::Component& component, int height = 24)
    {
        auto label = std::make_unique<juce::Label>();
        label->setText(text, juce::dontSendNotification);
        label->setColour(juce::Label::textColourId, juce::Colour(0xff9aa8ba));
        addAndMakeVisible(*label);
        addAndMakeVisible(component);
        rows.push_back({ std::move(label), &component, height, y });
        y += height + 6;
    }

    void buildValueEditor(const ns::Symbol& symbol)
    {
        auto setValue = [this](ns::PinDefaultValue v) {
            if (auto* s = symbol_()) { s->value = std::move(v); panel.changed(); }
        };
        auto numeric = [this, setValue](juce::TextEditor& editor, bool integer) {
            editor.setInputRestrictions(24, integer ? "-0123456789" : "-0123456789.");
            editor.onReturnKey = editor.onFocusLost = [&editor, setValue, integer]() {
                const auto text = editor.getText().trim();
                if (integer) setValue(static_cast<std::int64_t>(text.getLargeIntValue()));
                else setValue(text.getFloatValue());
            };
        };

        const auto& v = symbol.value;
        if (const auto* def = panel.enumOf(symbol))
        {
            for (size_t i = 0; i < def->variants.size(); ++i)
                choice.addItem(def->variants[i].name, static_cast<int>(i) + 1);
            if (const auto* i = std::get_if<std::int64_t>(&v))
                choice.setSelectedId(static_cast<int>(*i) + 1, juce::dontSendNotification);
            choice.onChange = [this, setValue]() { setValue(static_cast<std::int64_t>(choice.getSelectedId() - 1)); };
            row("Value", choice);
        }
        else if (symbol.type == ns::DataType::Bool)
        {
            toggle.setToggleState(std::holds_alternative<bool>(v) && std::get<bool>(v), juce::dontSendNotification);
            toggle.onClick = [this, setValue]() { setValue(toggle.getToggleState()); };
            row("Value", toggle);
        }
        else if (symbol.type == ns::DataType::String)
        {
            text.setText(std::holds_alternative<std::string>(v) ? juce::String(std::get<std::string>(v)) : juce::String(), juce::dontSendNotification);
            text.onReturnKey = text.onFocusLost = [this, setValue]() { setValue(text.getText().toStdString()); };
            row("Value", text);
        }
        else if (symbol.type == ns::DataType::Color || symbol.type == ns::DataType::Vec3)
        {
            const auto c = std::holds_alternative<ns::Vec3Default>(v) ? std::get<ns::Vec3Default>(v) : ns::Vec3Default {};
            juce::TextEditor* parts[3] = { &x, &y3, &z };
            const float values[3] = { c.x, c.y, c.z };
            for (int i = 0; i < 3; ++i)
            {
                parts[i]->setText(juce::String(values[i], 4), juce::dontSendNotification);
                parts[i]->setInputRestrictions(16, "-0123456789.");
                parts[i]->onReturnKey = parts[i]->onFocusLost = [this, setValue]() {
                    setValue(ns::Vec3Default { x.getText().getFloatValue(), y3.getText().getFloatValue(), z.getText().getFloatValue() });
                };
                vector.addAndMakeVisible(*parts[i]);
            }
            vector.onResize = [this]() {
                const int w = vector.getWidth() / 3;
                x.setBounds(0, 0, w - 4, vector.getHeight());
                y3.setBounds(w, 0, w - 4, vector.getHeight());
                z.setBounds(2 * w, 0, vector.getWidth() - 2 * w, vector.getHeight());
            };
            row(symbol.type == ns::DataType::Color ? "Value (RGB)" : "Value (XYZ)", vector);
        }
        else
        {
            const bool integer = symbol.type == ns::DataType::Int;
            if (const auto* f = std::get_if<float>(&v)) number.setText(juce::String(*f, 4), juce::dontSendNotification);
            if (const auto* i = std::get_if<std::int64_t>(&v)) number.setText(juce::String(*i), juce::dontSendNotification);
            numeric(number, integer);
            row("Value", number);
        }
    }

    // Three boxes side by side, laid out by the owning Editor.
    struct Row3 final : public juce::Component
    {
        std::function<void()> onResize;
        void resized() override { if (onResize) onResize(); }
    };

    struct RowEntry
    {
        std::unique_ptr<juce::Label> label;
        juce::Component* component;
        int height;
        int top;
    };

    SymbolsPanel& panel;
    std::string id;
    std::vector<RowEntry> rows;
    int y = 0;
    juce::TextEditor name, number, text, description, x, y3, z;
    juce::ComboBox kind, type, access, choice;
    juce::ToggleButton toggle, persistent;
    Row3 vector;
    juce::Label usesLabel;
    juce::TextButton remove { "Remove" };
};

//==============================================================================
SymbolsPanel::SymbolsPanel(ns::Graph& g)
    : graph(g),
      allowedTypes { ns::DataType::Float, ns::DataType::Int, ns::DataType::Bool, ns::DataType::Color, ns::DataType::String }
{
    title.setText("Variables", juce::dontSendNotification);
    title.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    addButton.setTooltip("Add a param, constant or variable");
    addButton.onClick = [this]() { showAddMenu(); };
    addAndMakeVisible(addButton);

    list.setModel(this);
    list.setRowHeight(28);
    list.setColour(juce::ListBox::backgroundColourId, background);
    addAndMakeVisible(list);
}

SymbolsPanel::~SymbolsPanel() = default;

void SymbolsPanel::setAllowedTypes(std::vector<ns::DataType> types)
{
    allowedTypes = std::move(types);
    refresh();
}

void SymbolsPanel::setEnums(const ns::NodeTypeRegistry& registry)
{
    enumSource = &registry;
    refresh();
}

std::vector<SymbolsPanel::TypeChoice> SymbolsPanel::typeChoices() const
{
    std::vector<TypeChoice> choices;
    for (auto t : allowedTypes)
        choices.push_back({ t, {}, symbolTypeName(t) });
    // Enums in scope (TYPES.md): the graph's own first, then the project's, pods' and built-in ones; a name defined
    // closer in wins.
    std::vector<const ns::EnumDef*> inScope;
    for (const auto& e : graph.Enums())
        inScope.push_back(&e);
    if (enumSource != nullptr)
        for (const auto& e : enumSource->Enums())
            if (graph.FindEnum(e.name) == nullptr)
                inScope.push_back(&e);
    for (const auto* e : inScope)
            choices.push_back({ ns::DataType::Int, e->name, juce::String(e->displayName.empty() ? e->name : e->displayName) });
    return choices;
}

const ns::EnumDef* SymbolsPanel::enumOf(const ns::Symbol& symbol) const
{
    if (symbol.enumType.empty())
        return nullptr;
    if (const auto* own = graph.FindEnum(symbol.enumType))
        return own;
    return enumSource != nullptr ? enumSource->FindEnum(symbol.enumType) : nullptr;
}

juce::String SymbolsPanel::typeLabel(const ns::Symbol& symbol) const
{
    if (const auto* def = enumOf(symbol))
        return def->displayName.empty() ? juce::String(def->name) : juce::String(def->displayName);
    return symbolTypeName(symbol.type);
}

void SymbolsPanel::refresh()
{
    list.updateContent();
    int row = -1;
    for (int i = 0; i < static_cast<int>(graph.Symbols().size()); ++i)
        if (juce::String(graph.Symbols()[static_cast<size_t>(i)].id) == selectedId)
            row = i;
    if (row < 0)
        selectedId.clear();
    list.selectRow(row, true, true);
    editor.reset();
    if (selectedId.isNotEmpty())
    {
        editor = std::make_unique<Editor>(*this, selectedId.toStdString());
        addAndMakeVisible(*editor);
    }
    resized();
    repaint();
}

void SymbolsPanel::graphChanged()
{
    list.repaint();
    if (editor != nullptr)
        editor->updateUses();
}

void SymbolsPanel::changed()
{
    list.repaint();
    if (onSymbolsChanged)
        onSymbolsChanged();
}

void SymbolsPanel::paint(juce::Graphics& g)
{
    g.fillAll(background);
    if (graph.Symbols().empty())
    {
        g.setColour(juce::Colour(0xff758294));
        g.setFont(juce::FontOptions(12.0f));
        g.drawFittedText("No params, constants or variables yet. Use + to add one, then drag it onto the graph.",
                         getLocalBounds().reduced(10).withTrimmedTop(34).removeFromTop(40), juce::Justification::topLeft, 3);
    }
}

void SymbolsPanel::resized()
{
    auto area = getLocalBounds().reduced(6);
    auto top = area.removeFromTop(26);
    addButton.setBounds(top.removeFromRight(30));
    title.setBounds(top);
    area.removeFromTop(4);
    const int editorHeight = editor != nullptr ? juce::jmin(editor->preferredHeight(), area.getHeight() / 2 + 120) : 0;
    if (editor != nullptr)
    {
        editor->setBounds(area.removeFromBottom(editorHeight));
        area.removeFromBottom(6);
    }
    list.setBounds(area);
}

int SymbolsPanel::getNumRows()
{
    return static_cast<int>(graph.Symbols().size());
}

void SymbolsPanel::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow(row, static_cast<int>(graph.Symbols().size())))
        return;
    const auto& symbol = graph.Symbols()[static_cast<size_t>(row)];
    auto area = juce::Rectangle<int>(0, 0, width, height).reduced(2, 2);
    g.setColour(selected ? juce::Colour(0xff2f5d8a) : rowColour);
    g.fillRoundedRectangle(area.toFloat(), 4.0f);

    auto badge = area.removeFromLeft(22).reduced(3);
    g.setColour(kindColour(symbol.kind));
    g.fillRoundedRectangle(badge.toFloat(), 3.0f);
    g.setColour(juce::Colours::black);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText(symbolKindName(symbol.kind).substring(0, 1), badge, juce::Justification::centred);

    auto text = area.reduced(6, 0);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText(symbol.name, text.removeFromLeft(text.getWidth() * 5 / 10), juce::Justification::centredLeft, true);
    g.setColour(juce::Colour(0xff7fffd4));
    g.setFont(juce::FontOptions(11.0f));
    const auto* def = enumOf(symbol);
    const auto* index = std::get_if<std::int64_t>(&symbol.value);
    const auto value = def != nullptr && index != nullptr ? juce::String(ns::EnumVariantName(*def, *index)) : valueSummary(symbol.value);
    g.drawText(typeLabel(symbol) + "  " + value, text, juce::Justification::centredRight, true);
}

void SymbolsPanel::selectedRowsChanged(int row)
{
    const auto newId = juce::isPositiveAndBelow(row, static_cast<int>(graph.Symbols().size()))
                         ? juce::String(graph.Symbols()[static_cast<size_t>(row)].id) : juce::String();
    if (newId == selectedId)
        return;
    selectedId = newId;
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<SymbolsPanel>(this)]() {
        if (safe != nullptr) safe->refresh();
    });
}

juce::var SymbolsPanel::getDragSourceDescription(const juce::SparseSet<int>& rows)
{
    if (rows.isEmpty() || ! juce::isPositiveAndBelow(rows[0], static_cast<int>(graph.Symbols().size())))
        return {};
    return juce::String(kSymbolDragPrefix) + juce::String(graph.Symbols()[static_cast<size_t>(rows[0])].id);
}

void SymbolsPanel::showAddMenu()
{
    juce::PopupMenu menu;
    const ns::SymbolKind kinds[3] = { ns::SymbolKind::Param, ns::SymbolKind::Constant, ns::SymbolKind::Variable };
    const char* hints[3] = { " - an input set from outside", " - a named fixed value", " - state the graph can change" };
    const auto choices = typeChoices();
    for (int k = 0; k < 3; ++k)
    {
        juce::PopupMenu types, enums;
        for (size_t t = 0; t < choices.size(); ++t)
            (choices[t].enumType.empty() ? types : enums).addItem(1000 * (k + 1) + static_cast<int>(t), choices[t].label);
        if (enums.getNumItems() > 0)
            types.addSubMenu("Choice", enums);
        menu.addSubMenu(symbolKindName(kinds[k]) + hints[k], types);
    }
    // A button-triggered menu opens at its button.
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addButton), [this, kinds, choices](int result) {
        if (result < 1000)
            return;
        const int k = result / 1000 - 1;
        const int t = result % 1000;
        if (juce::isPositiveAndBelow(k, 3) && juce::isPositiveAndBelow(t, static_cast<int>(choices.size())))
            addSymbol(kinds[k], choices[static_cast<size_t>(t)]);
    });
}

void SymbolsPanel::addSymbol(ns::SymbolKind kind, const TypeChoice& type)
{
    ns::Symbol symbol;
    symbol.kind = kind;
    symbol.type = type.type;
    symbol.enumType = type.enumType;
    symbol.value = ns::DefaultValueFor(type.type);
    symbol.name = (symbolKindName(kind) + " " + juce::String(static_cast<int>(graph.Symbols().size()) + 1)).toStdString();
    symbol.id = ns::MakeSymbolId(symbol.name, graph.Symbols());
    symbol.accessibility = kind == ns::SymbolKind::Param ? "public" : "graph";
    if (! graph.AddSymbol(symbol))
        return;
    selectedId = juce::String(symbol.id);
    changed();
    refresh();
}

} // namespace creation::node_editor_ui
