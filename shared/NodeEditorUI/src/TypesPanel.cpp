#include <creation/node_editor_ui/TypesPanel.h>

#include <node_system/symbol_nodes.h>

#include <algorithm>

namespace ns = ce::node_system;

namespace creation::node_editor_ui
{
namespace
{
const juce::Colour background { 0xff15181d };
const juce::Colour rowColour { 0xff1b232c };
const juce::Colour headerColour { 0xff20262f };
const juce::Colour enumBadge { 0xffb48cff };

juce::String scopeName(ns::TypeScope scope)
{
    switch (scope)
    {
        case ns::TypeScope::graph: return "This graph";
        case ns::TypeScope::project: return "Project";
        case ns::TypeScope::pod: return "Pod";
        case ns::TypeScope::builtin: return "Built-in";
    }
    return {};
}

// Colours a value can take: none, then a small palette that reads well on the dark UI.
const std::pair<const char*, std::uint32_t> valueColours[] = {
    { "No colour", 0u },          { "Red", 0xffe0605au },  { "Orange", 0xffe8964au }, { "Yellow", 0xffe8c84au },
    { "Green", 0xff6cc46au },     { "Teal", 0xff4ac0b0u }, { "Blue", 0xff5a9ce0u },   { "Purple", 0xffa47ae0u },
    { "Pink", 0xffe07ab4u },      { "Grey", 0xff9aa8bau },
};
}

//==============================================================================
// Edits one enum: its name, description and values (each with a colour, name and description, moved up or down,
// removed), shows where it is used and the FRust it compiles to. Read-only for built-in enums.
class TypesPanel::EnumEditor final : public juce::Component
{
public:
    EnumEditor(TypesPanel& p, ns::TypeScope s, std::string n) : panel(p), scope(s), name(std::move(n))
    {
        const auto* def = panel.findEnum(scope, name);
        if (def == nullptr)
            return;
        const bool editable = scope == ns::TypeScope::graph || (scope == ns::TypeScope::project && panel.projectEditable);

        displayName.setText(def->displayName, juce::dontSendNotification);
        displayName.setReadOnly(! editable);
        displayName.onReturnKey = displayName.onFocusLost = [this]() {
            if (auto* d = mutableDef(); d != nullptr && d->displayName != displayName.getText().trim().toStdString())
            {
                d->displayName = displayName.getText().trim().toStdString();
                panel.enumChanged(scope);
                panel.list.repaint();
            }
        };
        addLabelled("Name", displayName);

        identity.setText("FRust name " + juce::String(def->name) + "   -   " + scopeName(scope), juce::dontSendNotification);
        identity.setColour(juce::Label::textColourId, juce::Colour(0xff7f8ea3));
        addAndMakeVisible(identity);

        description.setMultiLine(true);
        description.setReturnKeyStartsNewLine(false);
        description.setReadOnly(! editable);
        description.setText(def->description, juce::dontSendNotification);
        description.onReturnKey = description.onFocusLost = [this]() {
            if (auto* d = mutableDef(); d != nullptr && d->description != description.getText().toStdString())
            {
                d->description = description.getText().toStdString();
                panel.enumChanged(scope);
            }
        };
        addLabelled("Description", description);

        valuesTitle.setText("Values - each is its position: the first is 0", juce::dontSendNotification);
        valuesTitle.setColour(juce::Label::textColourId, juce::Colour(0xff9aa8ba));
        addAndMakeVisible(valuesTitle);

        for (size_t i = 0; i < def->variants.size(); ++i)
            values.push_back(std::make_unique<ValueRow>(*this, static_cast<int>(i), def->variants[i], editable,
                                                        static_cast<int>(def->variants.size())));
        for (auto& row : values)
            addAndMakeVisible(*row);

        addValue.setEnabled(editable);
        addValue.onClick = [this]() {
            if (auto* d = mutableDef())
            {
                d->variants.push_back(ns::EnumVariant("Value " + std::to_string(d->variants.size() + 1)));
                changedShape();
            }
        };
        addAndMakeVisible(addValue);

        const int uses = panel.usesInGraph(name);
        usedBy.setText(uses == 0 ? juce::String("Not used in this graph yet - make a param of it in Variables (Choice).")
                                 : "Used " + juce::String(uses) + " time(s) in this graph.",
                       juce::dontSendNotification);
        usedBy.setColour(juce::Label::textColourId, juce::Colour(0xff7f8ea3));
        addAndMakeVisible(usedBy);

        frust.setText(ns::FrustEnumDeclaration(*def), juce::dontSendNotification);
        frust.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
        frust.setColour(juce::Label::textColourId, juce::Colour(0xff7fffd4));
        frust.setTooltip("What this enum compiles to in FRust");
        addAndMakeVisible(frust);

        remove.setEnabled(editable);
        remove.setTooltip(uses == 0 ? "Remove this type" : "Remove this type - what uses it in this graph becomes a plain integer");
        remove.onClick = [this]() { panel.removeType(scope, name); };
        addAndMakeVisible(remove);
    }

    int preferredHeight() const
    {
        return 30 + 22 + 50 + 24 + static_cast<int>(values.size()) * 30 + 34 + 24 + 24 + 36;
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto line = [&area](int height) { auto r = area.removeFromTop(height); area.removeFromTop(4); return r; };
        layoutLabelled(line(26), displayLabel, displayName);
        identity.setBounds(line(18));
        layoutLabelled(line(46), descriptionLabel, description);
        valuesTitle.setBounds(line(20));
        for (auto& row : values)
            row->setBounds(line(26));
        addValue.setBounds(line(28).removeFromLeft(110));
        usedBy.setBounds(line(20));
        frust.setBounds(line(20));
        remove.setBounds(line(28).removeFromRight(130));
    }

    // A value row: colour, name, description, up, down, remove.
    class ValueRow final : public juce::Component
    {
    public:
        ValueRow(EnumEditor& e, int i, const ns::EnumVariant& variant, bool editable, int count) : editor(e), index(i)
        {
            swatch.setColour(juce::TextButton::buttonColourId, variant.colour != 0 ? juce::Colour(variant.colour) : juce::Colour(0xff2a313b));
            swatch.setTooltip("The value's colour - shown on Switch cases and dropdowns");
            swatch.setEnabled(editable);
            swatch.onClick = [this]() { chooseColour(); };
            addAndMakeVisible(swatch);

            valueName.setText(variant.name, juce::dontSendNotification);
            valueName.setReadOnly(! editable);
            valueName.setTooltip("Value " + juce::String(index));
            valueName.onReturnKey = valueName.onFocusLost = [this]() {
                const auto text = valueName.getText().trim().toStdString();
                if (auto* d = editor.mutableDef(); d != nullptr && index < static_cast<int>(d->variants.size()) && ! text.empty()
                                                    && d->variants[static_cast<size_t>(index)].name != text)
                {
                    d->variants[static_cast<size_t>(index)].name = text;
                    editor.changedText();
                }
            };
            addAndMakeVisible(valueName);

            valueDescription.setText(variant.description, juce::dontSendNotification);
            valueDescription.setTextToShowWhenEmpty("description", juce::Colour(0xff5f6b7a));
            valueDescription.setReadOnly(! editable);
            valueDescription.onReturnKey = valueDescription.onFocusLost = [this]() {
                const auto text = valueDescription.getText().toStdString();
                if (auto* d = editor.mutableDef(); d != nullptr && index < static_cast<int>(d->variants.size())
                                                    && d->variants[static_cast<size_t>(index)].description != text)
                {
                    d->variants[static_cast<size_t>(index)].description = text;
                    editor.changedText();
                }
            };
            addAndMakeVisible(valueDescription);

            for (auto* b : { &up, &down, &cross })
            {
                b->setEnabled(editable);
                addAndMakeVisible(*b);
            }
            up.setEnabled(editable && index > 0);
            down.setEnabled(editable && index < count - 1);
            up.onClick = [this]() { editor.moveValue(index, index - 1); };
            down.onClick = [this]() { editor.moveValue(index, index + 1); };
            cross.setTooltip("Remove this value - what had it in this graph takes the first value");
            cross.onClick = [this]() { editor.removeValue(index); };
        }

        void resized() override
        {
            auto area = getLocalBounds();
            swatch.setBounds(area.removeFromLeft(22).reduced(2));
            area.removeFromLeft(4);
            cross.setBounds(area.removeFromRight(24));
            down.setBounds(area.removeFromRight(24));
            up.setBounds(area.removeFromRight(24));
            area.removeFromRight(4);
            valueName.setBounds(area.removeFromLeft(area.getWidth() * 4 / 10));
            area.removeFromLeft(4);
            valueDescription.setBounds(area);
        }

    private:
        void chooseColour()
        {
            juce::PopupMenu menu;
            int id = 1;
            for (const auto& [label, colour] : valueColours)
                menu.addColouredItem(id++, label, colour != 0 ? juce::Colour(colour) : juce::Colours::grey);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&swatch), [this](int chosen) {
                if (chosen <= 0)
                    return;
                if (auto* d = editor.mutableDef(); d != nullptr && index < static_cast<int>(d->variants.size()))
                {
                    d->variants[static_cast<size_t>(index)].colour = valueColours[chosen - 1].second;
                    editor.changedShape();
                }
            });
        }

        EnumEditor& editor;
        int index;
        juce::TextButton swatch, up { "^" }, down { "v" }, cross { "x" };
        juce::TextEditor valueName, valueDescription;
    };

private:
    friend class ValueRow;

    ns::EnumDef* mutableDef() { return panel.editableEnum(scope, name); }

    // Text changed in place: tell the owner and update the FRust shown, without rebuilding (typing goes on).
    void changedText()
    {
        panel.enumChanged(scope);
        if (const auto* def = panel.findEnum(scope, name))
            frust.setText(ns::FrustEnumDeclaration(*def), juce::dontSendNotification);
    }

    // Values added, moved, removed or recoloured: tell the owner, then rebuild after this click has returned.
    void changedShape()
    {
        panel.enumChanged(scope);
        panel.refreshSoon();
    }

    void moveValue(int from, int to)
    {
        auto* d = mutableDef();
        if (d == nullptr || to < 0 || to >= static_cast<int>(d->variants.size()))
            return;
        std::vector<int> mapping(d->variants.size());
        for (size_t i = 0; i < mapping.size(); ++i)
            mapping[i] = static_cast<int>(i);
        mapping[static_cast<size_t>(from)] = to;
        mapping[static_cast<size_t>(to)] = from;
        std::swap(d->variants[static_cast<size_t>(from)], d->variants[static_cast<size_t>(to)]);
        panel.remapValues(name, mapping);
        changedShape();
    }

    void removeValue(int at)
    {
        auto* d = mutableDef();
        if (d == nullptr || at < 0 || at >= static_cast<int>(d->variants.size()) || d->variants.size() <= 1)
            return;
        std::vector<int> mapping(d->variants.size());
        for (int i = 0; i < static_cast<int>(mapping.size()); ++i)
            mapping[static_cast<size_t>(i)] = i < at ? i : (i == at ? -1 : i - 1);
        d->variants.erase(d->variants.begin() + at);
        panel.remapValues(name, mapping);
        changedShape();
    }

    void addLabelled(const juce::String& text, juce::Component& component)
    {
        auto& label = labels.emplace_back(std::make_unique<juce::Label>());
        label->setText(text, juce::dontSendNotification);
        label->setColour(juce::Label::textColourId, juce::Colour(0xff9aa8ba));
        addAndMakeVisible(*label);
        addAndMakeVisible(component);
        if (&component == &displayName) displayLabel = label.get();
        if (&component == &description) descriptionLabel = label.get();
    }

    static void layoutLabelled(juce::Rectangle<int> row, juce::Label* label, juce::Component& component)
    {
        if (label != nullptr)
            label->setBounds(row.removeFromLeft(84));
        component.setBounds(row);
    }

    TypesPanel& panel;
    ns::TypeScope scope;
    std::string name;
    std::vector<std::unique_ptr<juce::Label>> labels;
    juce::Label* displayLabel = nullptr;
    juce::Label* descriptionLabel = nullptr;
    juce::TextEditor displayName, description;
    juce::Label identity, valuesTitle, usedBy, frust;
    std::vector<std::unique_ptr<ValueRow>> values;
    juce::TextButton addValue { "+ Add value" }, remove { "Remove type" };
};

//==============================================================================
TypesPanel::TypesPanel(ns::Graph& g, const ns::NodeTypeRegistry& r) : graph(g), registry(r)
{
    title.setText("Types", juce::dontSendNotification);
    title.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    addButton.setTooltip("Make a type: an enum, in this graph or the project");
    addButton.onClick = [this]() { showAddMenu(); };
    addAndMakeVisible(addButton);

    list.setModel(this);
    list.setRowHeight(26);
    list.setColour(juce::ListBox::backgroundColourId, background);
    addAndMakeVisible(list);
    addAndMakeVisible(editorView);
    editorView.setScrollBarsShown(true, false);
    refresh();
}

TypesPanel::~TypesPanel()
{
    editorView.setViewedComponent(nullptr, false);
}

void TypesPanel::setProjectTypes(std::vector<ns::EnumDef> enums, bool editable)
{
    projectEnums = std::move(enums);
    for (auto& def : projectEnums)
        def.scope = ns::TypeScope::project;
    projectEditable = editable;
    refresh();
}

const ns::EnumDef* TypesPanel::findEnum(ns::TypeScope scope, const std::string& name) const
{
    if (scope == ns::TypeScope::graph)
        return graph.FindEnum(name);
    if (scope == ns::TypeScope::project)
    {
        for (const auto& def : projectEnums)
            if (def.name == name)
                return &def;
        return nullptr;
    }
    return registry.FindEnum(name);
}

ns::EnumDef* TypesPanel::editableEnum(ns::TypeScope scope, const std::string& name)
{
    if (scope == ns::TypeScope::graph)
        return graph.FindEnum(name);
    if (scope == ns::TypeScope::project && projectEditable)
        for (auto& def : projectEnums)
            if (def.name == name)
                return &def;
    return nullptr;
}

void TypesPanel::enumChanged(ns::TypeScope scope)
{
    if (scope == ns::TypeScope::project && onProjectTypesChanged)
        onProjectTypesChanged(projectEnums);
    if (onGraphTypesChanged)
        onGraphTypesChanged(); // project types are in this graph's scope too
}

int TypesPanel::usesInGraph(const std::string& name) const
{
    int uses = 0;
    for (const auto& symbol : graph.Symbols())
        uses += symbol.enumType == name ? 1 : 0;
    for (const auto& [id, node] : graph.Nodes())
        for (const auto& pin : node->Inputs())
            if (const auto* def = ns::PinEnum(graph, registry, *node, pin); def != nullptr && def->name == name)
                ++uses;
    return uses;
}

void TypesPanel::remapValues(const std::string& name, const std::vector<int>& mapping)
{
    auto remap = [&mapping](ns::PinDefaultValue& value) {
        if (auto* v = std::get_if<std::int64_t>(&value); v != nullptr && *v >= 0 && *v < static_cast<std::int64_t>(mapping.size()))
            *v = std::max(0, mapping[static_cast<size_t>(*v)]);
    };
    for (const auto& symbol : graph.Symbols())
        if (symbol.enumType == name)
            remap(graph.FindSymbol(symbol.id)->value);
    for (const auto& [id, node] : graph.Nodes())
        for (const auto& pin : node->Inputs())
            if (const auto* def = ns::PinEnum(graph, registry, *node, pin); def != nullptr && def->name == name)
                remap(node->FindPin(pin.id)->defaultValue);
}

void TypesPanel::forgetEnum(const std::string& name)
{
    for (const auto& symbol : graph.Symbols())
        if (symbol.enumType == name)
            graph.FindSymbol(symbol.id)->enumType.clear();
    for (const auto& [id, node] : graph.Nodes())
    {
        if (ns::IsSymbolGetNode(node->TypeName()))
            if (const auto* symbol = ns::SymbolForGetNode(graph, *node))
                ns::BindSymbolGetNode(*node, *symbol);
        for (const auto& pin : node->Inputs())
            if (pin.type.enumType == name)
                node->FindPin(pin.id)->type.enumType.clear();
    }
}

void TypesPanel::removeType(ns::TypeScope scope, const std::string& name)
{
    if (scope == ns::TypeScope::graph)
        graph.RemoveEnum(name);
    else if (scope == ns::TypeScope::project && projectEditable)
        projectEnums.erase(std::remove_if(projectEnums.begin(), projectEnums.end(), [&name](const ns::EnumDef& e) { return e.name == name; }),
                           projectEnums.end());
    else
        return;
    forgetEnum(name);
    selectedName.clear();
    enumChanged(scope);
    refreshSoon();
}

void TypesPanel::showAddMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "New enum in this graph");
    menu.addItem(2, "New enum in the project - for every graph in it", projectEditable);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addButton), [this](int result) {
        if (result == 1) addEnum(ns::TypeScope::graph);
        if (result == 2) addEnum(ns::TypeScope::project);
    });
}

void TypesPanel::addEnum(ns::TypeScope scope)
{
    // A name unique across everything in scope, so a new type never hides another.
    std::vector<ns::EnumDef> taken = graph.Enums();
    taken.insert(taken.end(), projectEnums.begin(), projectEnums.end());
    taken.insert(taken.end(), registry.Enums().begin(), registry.Enums().end());
    ns::EnumDef def;
    def.name = ns::MakeEnumName("New Enum", taken);
    def.displayName = "New Enum";
    def.variants = { "First", "Second" };
    def.scope = scope;
    if (scope == ns::TypeScope::graph)
    {
        if (! graph.AddEnum(def))
            return;
    }
    else if (projectEditable)
        projectEnums.push_back(def);
    else
        return;
    selectedScope = scope;
    selectedName = def.name;
    enumChanged(scope);
    refresh();
}

void TypesPanel::rebuildRows()
{
    rows.clear();
    auto group = [this](ns::TypeScope scope, const std::vector<const ns::EnumDef*>& defs, bool always) {
        if (defs.empty() && ! always)
            return;
        rows.push_back({ true, scope, {}, scopeName(scope) });
        for (const auto* def : defs)
            rows.push_back({ false, scope, def->name, juce::String(def->displayName.empty() ? def->name : def->displayName) });
    };
    std::vector<const ns::EnumDef*> own, project, builtin;
    for (const auto& def : graph.Enums())
        own.push_back(&def);
    for (const auto& def : projectEnums)
        project.push_back(&def);
    for (const auto& def : registry.Enums())
        if (def.scope != ns::TypeScope::project)
            builtin.push_back(&def);
    group(ns::TypeScope::graph, own, true);
    group(ns::TypeScope::project, project, true);
    group(ns::TypeScope::builtin, builtin, false);
}

void TypesPanel::refresh()
{
    rebuildRows();
    list.updateContent();
    int selectedRow = -1;
    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
        if (! rows[static_cast<size_t>(i)].header && rows[static_cast<size_t>(i)].scope == selectedScope && rows[static_cast<size_t>(i)].name == selectedName)
            selectedRow = i;
    if (selectedRow < 0)
        selectedName.clear();
    list.selectRow(selectedRow, true, true);
    editorView.setViewedComponent(nullptr, false);
    editor.reset();
    if (! selectedName.empty() && findEnum(selectedScope, selectedName) != nullptr)
    {
        editor = std::make_unique<EnumEditor>(*this, selectedScope, selectedName);
        editorView.setViewedComponent(editor.get(), false);
    }
    resized();
    repaint();
}

void TypesPanel::refreshSoon()
{
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<TypesPanel>(this)]() {
        if (safe != nullptr)
            safe->refresh();
    });
}

void TypesPanel::paint(juce::Graphics& g)
{
    g.fillAll(background);
}

void TypesPanel::resized()
{
    auto area = getLocalBounds().reduced(6);
    auto top = area.removeFromTop(26);
    addButton.setBounds(top.removeFromRight(30));
    title.setBounds(top);
    area.removeFromTop(4);
    const int listHeight = editor != nullptr ? juce::jmin(area.getHeight() / 3, juce::jmax(80, static_cast<int>(rows.size()) * 26)) : area.getHeight();
    list.setBounds(area.removeFromTop(listHeight));
    area.removeFromTop(6);
    editorView.setBounds(area);
    if (editor != nullptr)
        editor->setSize(juce::jmax(10, area.getWidth() - editorView.getScrollBarThickness()), editor->preferredHeight());
}

int TypesPanel::getNumRows()
{
    return static_cast<int>(rows.size());
}

void TypesPanel::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow(row, static_cast<int>(rows.size())))
        return;
    const auto& r = rows[static_cast<size_t>(row)];
    auto area = juce::Rectangle<int>(0, 0, width, height);
    if (r.header)
    {
        g.setColour(headerColour);
        g.fillRect(area);
        g.setColour(juce::Colour(0xff9aa8ba));
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText(r.title, area.reduced(8, 0), juce::Justification::centredLeft, true);
        return;
    }
    area = area.reduced(2, 2);
    g.setColour(selected ? juce::Colour(0xff2f5d8a) : rowColour);
    g.fillRoundedRectangle(area.toFloat(), 4.0f);
    auto badge = area.removeFromLeft(22).reduced(3);
    g.setColour(enumBadge);
    g.fillRoundedRectangle(badge.toFloat(), 3.0f);
    g.setColour(juce::Colours::black);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("E", badge, juce::Justification::centred);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(13.0f));
    auto text = area.reduced(6, 0);
    g.drawText(r.title, text, juce::Justification::centredLeft, true);
    if (const auto* def = findEnum(r.scope, r.name))
    {
        g.setColour(juce::Colour(0xff7f8ea3));
        g.setFont(juce::FontOptions(11.0f));
        g.drawText(juce::String(def->variants.size()) + " values", text, juce::Justification::centredRight, true);
    }
}

void TypesPanel::selectedRowsChanged(int row)
{
    if (! juce::isPositiveAndBelow(row, static_cast<int>(rows.size())) || rows[static_cast<size_t>(row)].header)
        return;
    const auto& r = rows[static_cast<size_t>(row)];
    if (r.scope == selectedScope && r.name == selectedName)
        return;
    selectedScope = r.scope;
    selectedName = r.name;
    refreshSoon();
}

} // namespace creation::node_editor_ui
