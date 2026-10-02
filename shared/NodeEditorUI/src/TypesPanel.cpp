#include <creation/node_editor_ui/TypesPanel.h>

#include <node_system/symbol_nodes.h>
#include <node_system/struct_nodes.h>

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
const juce::Colour structBadge { 0xff4f7cf0 };

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
class TypesPanel::TypeEditor : public juce::Component
{
public:
    virtual int preferredHeight() const = 0;
};

//==============================================================================
// Edits one enum: its name, description and values (each with a colour, name and description, moved up or down,
// removed), shows where it is used and the FRust it compiles to. Read-only for built-in enums.
class TypesPanel::EnumEditor final : public TypeEditor
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

    int preferredHeight() const override
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
// Edits one struct: its name, description and members - each with a name, a type (any value type, an image, drawing
// or brush, an enum or another struct in scope), a default and a description, moved up or down, removed, added -
// shows where it is used and the FRust it compiles to. Read-only for built-in structs.
class TypesPanel::StructEditor final : public TypeEditor
{
public:
    // A type a member can have, as the type list shows it.
    struct TypeOption
    {
        juce::String label;
        ns::PinTypeDesc type;
    };

    StructEditor(TypesPanel& p, ns::TypeScope s, std::string n) : panel(p), scope(s), name(std::move(n))
    {
        const auto* def = panel.findStruct(scope, name);
        if (def == nullptr)
            return;
        editable = scope == ns::TypeScope::graph || (scope == ns::TypeScope::project && panel.projectEditable);
        buildTypeOptions();

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
        addLabel(displayLabel, "Name");
        addAndMakeVisible(displayName);

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
        addLabel(descriptionLabel, "Description");
        addAndMakeVisible(description);

        membersTitle.setText("Members - name, type, default", juce::dontSendNotification);
        membersTitle.setColour(juce::Label::textColourId, juce::Colour(0xff9aa8ba));
        addAndMakeVisible(membersTitle);

        for (size_t i = 0; i < def->members.size(); ++i)
            members.push_back(std::make_unique<MemberRow>(*this, static_cast<int>(i), def->members[i], static_cast<int>(def->members.size())));
        for (auto& row : members)
            addAndMakeVisible(*row);

        problem.setColour(juce::Label::textColourId, juce::Colour(0xffff8a80));
        addAndMakeVisible(problem);

        addMember.setEnabled(editable);
        addMember.onClick = [this]() {
            if (auto* d = mutableDef())
            {
                ns::StructMember member;
                int n = static_cast<int>(d->members.size()) + 1;
                auto taken = [d](const std::string& candidate) {
                    return std::any_of(d->members.begin(), d->members.end(), [&candidate](const ns::StructMember& m) { return m.name == candidate; });
                };
                while (taken("member " + std::to_string(n)))
                    ++n;
                member.name = "member " + std::to_string(n);
                member.type = { ns::PinKind::Data, ns::DataType::Float };
                member.defaultValue = 0.0f;
                d->members.push_back(member);
                changedShape();
            }
        };
        addAndMakeVisible(addMember);

        const int uses = panel.structUsesInGraph(name);
        usedBy.setText(uses == 0 ? juce::String("Not used in this graph yet - make a param of it in Variables, or use Make Struct.")
                                 : "Used " + juce::String(uses) + " time(s) in this graph.",
                       juce::dontSendNotification);
        usedBy.setColour(juce::Label::textColourId, juce::Colour(0xff7f8ea3));
        addAndMakeVisible(usedBy);

        frust.setText(ns::FrustStructDeclaration(*def), juce::dontSendNotification);
        frust.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
        frust.setColour(juce::Label::textColourId, juce::Colour(0xff7fffd4));
        frust.setTooltip("What this struct compiles to in FRust");
        frust.setMinimumHorizontalScale(0.5f);
        addAndMakeVisible(frust);

        remove.setEnabled(editable);
        remove.setTooltip(uses == 0 ? "Remove this type" : "Remove this type - its params become numbers, its struct nodes lose their pins");
        remove.onClick = [this]() { panel.removeType(scope, name); };
        addAndMakeVisible(remove);
    }

    int preferredHeight() const override
    {
        return 30 + 22 + 50 + 24 + static_cast<int>(members.size()) * 60 + 22 + 34 + 24 + 36 + 36;
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto line = [&area](int height) { auto r = area.removeFromTop(height); area.removeFromTop(4); return r; };
        auto row = line(26);
        displayLabel.setBounds(row.removeFromLeft(84));
        displayName.setBounds(row);
        identity.setBounds(line(18));
        row = line(46);
        descriptionLabel.setBounds(row.removeFromLeft(84));
        description.setBounds(row);
        membersTitle.setBounds(line(20));
        for (auto& m : members)
            m->setBounds(line(56));
        problem.setBounds(line(18));
        addMember.setBounds(line(28).removeFromLeft(120));
        usedBy.setBounds(line(20));
        frust.setBounds(line(32));
        remove.setBounds(line(28).removeFromRight(130));
    }

    // A member: name, type, default on the first line; description and move / remove on the second.
    class MemberRow final : public juce::Component
    {
    public:
        MemberRow(StructEditor& e, int i, const ns::StructMember& member, int count) : editor(e), index(i)
        {
            memberName.setText(member.name, juce::dontSendNotification);
            memberName.setReadOnly(! editor.editable);
            memberName.onReturnKey = memberName.onFocusLost = [this]() { editor.renameMember(index, memberName.getText().trim().toStdString()); };
            addAndMakeVisible(memberName);

            int selected = 0;
            for (size_t k = 0; k < editor.typeOptions.size(); ++k)
            {
                typeBox.addItem(editor.typeOptions[k].label, static_cast<int>(k) + 1);
                if (editor.typeOptions[k].type == member.type)
                    selected = static_cast<int>(k) + 1;
            }
            typeBox.setSelectedId(selected, juce::dontSendNotification);
            typeBox.setEnabled(editor.editable);
            typeBox.onChange = [this]() { editor.retypeMember(index, typeBox.getSelectedId() - 1); };
            addAndMakeVisible(typeBox);

            buildDefaultEditor(member);

            memberDescription.setText(member.description, juce::dontSendNotification);
            memberDescription.setTextToShowWhenEmpty("description", juce::Colour(0xff5f6b7a));
            memberDescription.setReadOnly(! editor.editable);
            memberDescription.onReturnKey = memberDescription.onFocusLost = [this]() {
                if (auto* d = editor.mutableDef(); d != nullptr && index < static_cast<int>(d->members.size())
                                                    && d->members[static_cast<size_t>(index)].description != memberDescription.getText().toStdString())
                {
                    d->members[static_cast<size_t>(index)].description = memberDescription.getText().toStdString();
                    editor.changedText();
                }
            };
            addAndMakeVisible(memberDescription);

            for (auto* b : { &up, &down, &cross })
            {
                b->setEnabled(editor.editable);
                addAndMakeVisible(*b);
            }
            up.setEnabled(editor.editable && index > 0);
            down.setEnabled(editor.editable && index < count - 1);
            up.onClick = [this]() { editor.moveMember(index, index - 1); };
            down.onClick = [this]() { editor.moveMember(index, index + 1); };
            cross.setTooltip("Remove this member");
            cross.onClick = [this]() { editor.removeMember(index); };
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(0, 1);
            auto top = area.removeFromTop(area.getHeight() / 2).reduced(0, 1);
            auto bottom = area.reduced(0, 1);
            const int third = top.getWidth() / 3;
            memberName.setBounds(top.removeFromLeft(third - 2));
            top.removeFromLeft(4);
            typeBox.setBounds(top.removeFromLeft(third - 2));
            top.removeFromLeft(4);
            if (defaultEditor != nullptr)
                defaultEditor->setBounds(top);
            cross.setBounds(bottom.removeFromRight(24));
            down.setBounds(bottom.removeFromRight(24));
            up.setBounds(bottom.removeFromRight(24));
            bottom.removeFromRight(4);
            memberDescription.setBounds(bottom);
        }

        void paint(juce::Graphics& g) override
        {
            g.setColour(juce::Colour(0xff1b232c));
            g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
        }

    private:
        // The default's editor follows the member's type: a number, a toggle, three numbers, text, an enum's values;
        // images, drawings, brushes and nested structs have no default.
        void buildDefaultEditor(const ns::StructMember& member)
        {
            const auto& t = member.type;
            auto commit = [this](ns::PinDefaultValue value) {
                if (auto* d = editor.mutableDef(); d != nullptr && index < static_cast<int>(d->members.size()))
                {
                    d->members[static_cast<size_t>(index)].defaultValue = std::move(value);
                    editor.changedText();
                }
            };
            if (! t.enumType.empty())
            {
                auto box = std::make_unique<juce::ComboBox>();
                if (const auto* def = ns::FindEnumFor(editor.panel.graph, editor.panel.registry, t.enumType))
                    for (size_t k = 0; k < def->variants.size(); ++k)
                        box->addItem(def->variants[k].name, static_cast<int>(k) + 1);
                if (const auto* v = std::get_if<std::int64_t>(&member.defaultValue))
                    box->setSelectedId(static_cast<int>(*v) + 1, juce::dontSendNotification);
                box->onChange = [commit, b = box.get()]() { commit(static_cast<std::int64_t>(b->getSelectedId() - 1)); };
                defaultEditor = std::move(box);
            }
            else if (t.dataType == ns::DataType::Bool)
            {
                auto toggle = std::make_unique<juce::ToggleButton>("on");
                toggle->setToggleState(std::holds_alternative<bool>(member.defaultValue) && std::get<bool>(member.defaultValue), juce::dontSendNotification);
                toggle->onClick = [commit, b = toggle.get()]() { commit(b->getToggleState()); };
                defaultEditor = std::move(toggle);
            }
            else if (t.dataType == ns::DataType::Float || t.dataType == ns::DataType::Int || t.dataType == ns::DataType::String
                     || t.dataType == ns::DataType::Color || t.dataType == ns::DataType::Vec3)
            {
                auto text = std::make_unique<juce::TextEditor>();
                juce::String shown;
                if (const auto* f = std::get_if<float>(&member.defaultValue)) shown = juce::String(*f);
                if (const auto* i = std::get_if<std::int64_t>(&member.defaultValue)) shown = juce::String(*i);
                if (const auto* s = std::get_if<std::string>(&member.defaultValue)) shown = juce::String(*s);
                if (const auto* v = std::get_if<ns::Vec3Default>(&member.defaultValue))
                    shown = juce::String(v->x) + ", " + juce::String(v->y) + ", " + juce::String(v->z);
                text->setText(shown, juce::dontSendNotification);
                text->setTooltip(t.dataType == ns::DataType::Color || t.dataType == ns::DataType::Vec3 ? "Default: three numbers, r, g, b"
                                                                                                    : "Default");
                const auto dataType = t.dataType;
                text->onReturnKey = text->onFocusLost = [commit, dataType, e = text.get()]() {
                    const auto value = e->getText().trim();
                    if (dataType == ns::DataType::Float) commit(value.getFloatValue());
                    else if (dataType == ns::DataType::Int) commit(static_cast<std::int64_t>(value.getLargeIntValue()));
                    else if (dataType == ns::DataType::String) commit(value.toStdString());
                    else
                    {
                        juce::StringArray parts;
                        parts.addTokens(value, ",", "");
                        commit(ns::Vec3Default { parts[0].getFloatValue(), parts[1].getFloatValue(), parts[2].getFloatValue() });
                    }
                };
                defaultEditor = std::move(text);
            }
            else
            {
                auto none = std::make_unique<juce::Label>();
                none->setText("no default", juce::dontSendNotification);
                none->setColour(juce::Label::textColourId, juce::Colour(0xff5f6b7a));
                defaultEditor = std::move(none);
            }
            defaultEditor->setEnabled(editor.editable);
            addAndMakeVisible(*defaultEditor);
        }

        StructEditor& editor;
        int index;
        juce::TextEditor memberName, memberDescription;
        juce::ComboBox typeBox;
        std::unique_ptr<juce::Component> defaultEditor;
        juce::TextButton up { "^" }, down { "v" }, cross { "x" };
    };

private:
    friend class MemberRow;

    ns::StructDef* mutableDef() { return panel.editableStruct(scope, name); }

    void buildTypeOptions()
    {
        const std::pair<const char*, ns::DataType> plain[] = {
            { "Number", ns::DataType::Float }, { "Integer", ns::DataType::Int }, { "Toggle", ns::DataType::Bool },
            { "Colour", ns::DataType::Color }, { "Vector", ns::DataType::Vec3 }, { "Text", ns::DataType::String },
            { "Image", ns::DataType::Texture }, { "Drawing", ns::DataType::Drawing }, { "Brush", ns::DataType::Brush },
        };
        for (const auto& [label, type] : plain)
            typeOptions.push_back({ label, { ns::PinKind::Data, type } });
        for (const auto* e : panel.enumsInScope())
        {
            ns::PinTypeDesc t { ns::PinKind::Data, ns::DataType::Int };
            t.enumType = e->name;
            typeOptions.push_back({ "Enum: " + juce::String(e->displayName.empty() ? e->name : e->displayName), t });
        }
        for (const auto* st : panel.structsInScope())
        {
            if (st->name == name)
                continue; // a struct cannot hold itself
            ns::PinTypeDesc t { ns::PinKind::Data, ns::DataType::Struct };
            t.structType = st->name;
            typeOptions.push_back({ "Struct: " + juce::String(st->displayName.empty() ? st->name : st->displayName), t });
        }
    }

    void changedText()
    {
        panel.enumChanged(scope);
        if (const auto* def = panel.findStruct(scope, name))
            frust.setText(ns::FrustStructDeclaration(*def), juce::dontSendNotification);
    }

    void changedShape()
    {
        panel.enumChanged(scope);
        panel.refreshSoon();
    }

    void renameMember(int at, const std::string& newName)
    {
        auto* d = mutableDef();
        if (d == nullptr || at < 0 || at >= static_cast<int>(d->members.size()) || d->members[static_cast<size_t>(at)].name == newName)
            return;
        if (ns::StructMemberNameReserved(newName))
        {
            problem.setText("\"" + juce::String(newName) + "\" is used by the struct nodes themselves - choose another name.", juce::dontSendNotification);
            return;
        }
        for (size_t k = 0; k < d->members.size(); ++k)
            if (static_cast<int>(k) != at && ns::StructMemberPinName(d->members[k].name) == ns::StructMemberPinName(newName))
            {
                problem.setText("There is already a member called \"" + juce::String(newName) + "\".", juce::dontSendNotification);
                return;
            }
        problem.setText({}, juce::dontSendNotification);
        panel.renameMemberInNodes(name, d->members[static_cast<size_t>(at)].name, newName);
        d->members[static_cast<size_t>(at)].name = newName;
        changedText();
    }

    void retypeMember(int at, int option)
    {
        auto* d = mutableDef();
        if (d == nullptr || at < 0 || at >= static_cast<int>(d->members.size()) || option < 0 || option >= static_cast<int>(typeOptions.size()))
            return;
        auto& member = d->members[static_cast<size_t>(at)];
        member.type = typeOptions[static_cast<size_t>(option)].type;
        member.defaultValue = ns::DefaultValueFor(member.type.dataType);
        changedShape();
    }

    void moveMember(int from, int to)
    {
        auto* d = mutableDef();
        if (d == nullptr || to < 0 || to >= static_cast<int>(d->members.size()))
            return;
        std::vector<int> mapping(d->members.size());
        for (size_t i = 0; i < mapping.size(); ++i)
            mapping[i] = static_cast<int>(i);
        mapping[static_cast<size_t>(from)] = to;
        mapping[static_cast<size_t>(to)] = from;
        std::swap(d->members[static_cast<size_t>(from)], d->members[static_cast<size_t>(to)]);
        panel.remapMembers(name, mapping);
        changedShape();
    }

    void removeMember(int at)
    {
        auto* d = mutableDef();
        if (d == nullptr || at < 0 || at >= static_cast<int>(d->members.size()))
            return;
        std::vector<int> mapping(d->members.size());
        for (int i = 0; i < static_cast<int>(mapping.size()); ++i)
            mapping[static_cast<size_t>(i)] = i < at ? i : (i == at ? -1 : i - 1);
        d->members.erase(d->members.begin() + at);
        panel.remapMembers(name, mapping);
        changedShape();
    }

    void addLabel(juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, juce::Colour(0xff9aa8ba));
        addAndMakeVisible(label);
    }

    TypesPanel& panel;
    ns::TypeScope scope;
    std::string name;
    bool editable = false;
    std::vector<TypeOption> typeOptions;
    juce::Label displayLabel, descriptionLabel, identity, membersTitle, problem, usedBy, frust;
    juce::TextEditor displayName, description;
    std::vector<std::unique_ptr<MemberRow>> members;
    juce::TextButton addMember { "+ Add member" }, remove { "Remove type" };
};

//==============================================================================
TypesPanel::TypesPanel(ns::Graph& g, const ns::NodeTypeRegistry& r) : graph(g), registry(r)
{
    title.setText("Types", juce::dontSendNotification);
    title.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    addButton.setTooltip("Make a type: an enum or a struct, in this graph or the project");
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

void TypesPanel::setProjectTypes(std::vector<ns::EnumDef> enums, std::vector<ns::StructDef> structs, bool editable)
{
    projectEnums = std::move(enums);
    for (auto& def : projectEnums)
        def.scope = ns::TypeScope::project;
    projectStructs = std::move(structs);
    for (auto& def : projectStructs)
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
        onProjectTypesChanged(projectEnums, projectStructs);
    if (onGraphTypesChanged)
        onGraphTypesChanged(); // project types are in this graph's scope too
}

const ns::StructDef* TypesPanel::findStruct(ns::TypeScope scope, const std::string& name) const
{
    if (scope == ns::TypeScope::graph)
        return graph.FindStruct(name);
    if (scope == ns::TypeScope::project)
    {
        for (const auto& def : projectStructs)
            if (def.name == name)
                return &def;
        return nullptr;
    }
    return registry.FindStruct(name);
}

ns::StructDef* TypesPanel::editableStruct(ns::TypeScope scope, const std::string& name)
{
    if (scope == ns::TypeScope::graph)
        return graph.FindStruct(name);
    if (scope == ns::TypeScope::project && projectEditable)
        for (auto& def : projectStructs)
            if (def.name == name)
                return &def;
    return nullptr;
}

std::vector<const ns::EnumDef*> TypesPanel::enumsInScope() const
{
    std::vector<const ns::EnumDef*> result;
    auto add = [&result](const ns::EnumDef& def) {
        if (std::none_of(result.begin(), result.end(), [&def](const ns::EnumDef* e) { return e->name == def.name; }))
            result.push_back(&def);
    };
    for (const auto& def : graph.Enums()) add(def);
    for (const auto& def : projectEnums) add(def);
    for (const auto& def : registry.Enums()) add(def);
    return result;
}

std::vector<const ns::StructDef*> TypesPanel::structsInScope() const
{
    std::vector<const ns::StructDef*> result;
    auto add = [&result](const ns::StructDef& def) {
        if (std::none_of(result.begin(), result.end(), [&def](const ns::StructDef* e) { return e->name == def.name; }))
            result.push_back(&def);
    };
    for (const auto& def : graph.Structs()) add(def);
    for (const auto& def : projectStructs) add(def);
    for (const auto& def : registry.Structs()) add(def);
    return result;
}

std::vector<std::string> TypesPanel::takenNames() const
{
    std::vector<std::string> names;
    for (const auto* e : enumsInScope()) names.push_back(e->name);
    for (const auto* s : structsInScope()) names.push_back(s->name);
    return names;
}

int TypesPanel::structUsesInGraph(const std::string& name) const
{
    int uses = 0;
    for (const auto& symbol : graph.Symbols())
        uses += symbol.structType == name ? 1 : 0;
    for (const auto& [id, node] : graph.Nodes())
        uses += ns::StructNodeKindOf(*node) != ns::StructNodeKind::none && ns::StructNodeType(*node) == name ? 1 : 0;
    return uses;
}

void TypesPanel::remapMembers(const std::string& name, const std::vector<int>& mapping)
{
    for (const auto& symbol : graph.Symbols())
    {
        if (symbol.structType != name)
            continue;
        auto& values = graph.FindSymbol(symbol.id)->memberValues;
        std::vector<ns::PinDefaultValue> remapped(mapping.size());
        for (size_t i = 0; i < mapping.size() && i < values.size(); ++i)
            if (mapping[i] >= 0 && static_cast<size_t>(mapping[i]) < remapped.size())
                remapped[static_cast<size_t>(mapping[i])] = values[i];
        size_t kept = 0;
        for (int m : mapping)
            kept += m >= 0 ? 1 : 0;
        remapped.resize(kept);
        values = std::move(remapped);
    }
}

void TypesPanel::renameMemberInNodes(const std::string& structName, const std::string& from, const std::string& to)
{
    // Set Members' ticks and Get Member's choice name members: they follow the rename, so their pins (and wires) stay.
    for (const auto& [id, node] : graph.Nodes())
    {
        const auto kind = ns::StructNodeKindOf(*node);
        if ((kind != ns::StructNodeKind::setMembers && kind != ns::StructNodeKind::getMember) || ns::StructNodeType(*node) != structName)
            continue;
        for (const auto& pin : node->Inputs())
        {
            if (pin.name != ns::kStructMembersPin && pin.name != ns::kStructMemberPin)
                continue;
            const auto* text = std::get_if<std::string>(&pin.defaultValue);
            if (text == nullptr)
                continue;
            juce::StringArray names;
            names.addTokens(juce::String(*text), ",", "");
            names.trim();
            for (auto& n : names)
                if (n == juce::String(from))
                    n = juce::String(to);
            node->FindPin(pin.id)->defaultValue = names.joinIntoString(",").toStdString();
        }
    }
}

void TypesPanel::forgetStruct(const std::string& name)
{
    // Struct params of it become plain numbers; struct nodes set to it lose their pins.
    for (const auto& symbol : graph.Symbols())
        if (symbol.structType == name)
        {
            auto* s = graph.FindSymbol(symbol.id);
            s->structType.clear();
            s->memberValues.clear();
            s->type = ns::DataType::Float;
            s->value = 0.0f;
        }
    std::vector<ns::NodeId> structNodes;
    for (const auto& [id, node] : graph.Nodes())
    {
        if (ns::IsSymbolGetNode(node->TypeName()))
            if (const auto* symbol = ns::SymbolForGetNode(graph, *node))
                ns::BindSymbolGetNode(*node, *symbol);
        if (ns::StructNodeKindOf(*node) != ns::StructNodeKind::none && ns::StructNodeType(*node) == name)
            structNodes.push_back(id);
    }
    for (auto id : structNodes)
    {
        auto* node = graph.FindNode(id);
        for (const auto& pin : node->Inputs())
            if (pin.name == ns::kStructTypePin)
                node->FindPin(pin.id)->defaultValue = std::string();
        ns::SyncStructNodePins(graph, registry, id);
    }
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
    if (findStruct(scope, name) != nullptr)
    {
        if (scope == ns::TypeScope::graph)
            graph.RemoveStruct(name);
        else if (scope == ns::TypeScope::project && projectEditable)
            projectStructs.erase(std::remove_if(projectStructs.begin(), projectStructs.end(), [&name](const ns::StructDef& d) { return d.name == name; }),
                                 projectStructs.end());
        else
            return;
        forgetStruct(name);
        selectedName.clear();
        enumChanged(scope);
        refreshSoon();
        return;
    }
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
    menu.addSectionHeader("Enum - a fixed set of named values");
    menu.addItem(1, "New enum in this graph");
    menu.addItem(2, "New enum in the project - for every graph in it", projectEditable);
    menu.addSectionHeader("Struct - a group of named members");
    menu.addItem(3, "New struct in this graph");
    menu.addItem(4, "New struct in the project - for every graph in it", projectEditable);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addButton), [this](int result) {
        if (result == 1) addEnum(ns::TypeScope::graph);
        if (result == 2) addEnum(ns::TypeScope::project);
        if (result == 3) addStruct(ns::TypeScope::graph);
        if (result == 4) addStruct(ns::TypeScope::project);
    });
}

void TypesPanel::addEnum(ns::TypeScope scope)
{
    // A name unique across everything in scope, so a new type never hides another.
    ns::EnumDef def;
    def.name = ns::MakeTypeName("New Enum", takenNames());
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
    selectedIsStruct = false;
    enumChanged(scope);
    refresh();
}

void TypesPanel::addStruct(ns::TypeScope scope)
{
    ns::StructDef def;
    def.name = ns::MakeTypeName("New Struct", takenNames());
    def.displayName = "New Struct";
    ns::StructMember first;
    first.name = "amount";
    first.type = { ns::PinKind::Data, ns::DataType::Float };
    first.defaultValue = 0.5f;
    def.members = { first };
    def.scope = scope;
    if (scope == ns::TypeScope::graph)
    {
        if (! graph.AddStruct(def))
            return;
    }
    else if (projectEditable)
        projectStructs.push_back(def);
    else
        return;
    selectedScope = scope;
    selectedName = def.name;
    selectedIsStruct = true;
    enumChanged(scope);
    refresh();
}

void TypesPanel::rebuildRows()
{
    rows.clear();
    auto titleOf = [](const auto& def) { return juce::String(def.displayName.empty() ? def.name : def.displayName); };
    auto group = [this, &titleOf](ns::TypeScope scope, const std::vector<const ns::EnumDef*>& enums, const std::vector<const ns::StructDef*>& structs,
                                bool always) {
        if (enums.empty() && structs.empty() && ! always)
            return;
        Row header;
        header.header = true;
        header.scope = scope;
        header.title = scopeName(scope);
        rows.push_back(header);
        for (const auto* def : enums)
            rows.push_back({ false, false, scope, def->name, titleOf(*def) });
        for (const auto* def : structs)
            rows.push_back({ false, true, scope, def->name, titleOf(*def) });
    };
    std::vector<const ns::EnumDef*> ownEnums, projectEnumRows, builtinEnums;
    std::vector<const ns::StructDef*> ownStructs, projectStructRows, builtinStructs;
    for (const auto& def : graph.Enums()) ownEnums.push_back(&def);
    for (const auto& def : projectEnums) projectEnumRows.push_back(&def);
    for (const auto& def : registry.Enums())
        if (def.scope != ns::TypeScope::project) builtinEnums.push_back(&def);
    for (const auto& def : graph.Structs()) ownStructs.push_back(&def);
    for (const auto& def : projectStructs) projectStructRows.push_back(&def);
    for (const auto& def : registry.Structs())
        if (def.scope != ns::TypeScope::project) builtinStructs.push_back(&def);
    group(ns::TypeScope::graph, ownEnums, ownStructs, true);
    group(ns::TypeScope::project, projectEnumRows, projectStructRows, true);
    group(ns::TypeScope::builtin, builtinEnums, builtinStructs, false);
}

void TypesPanel::refresh()
{
    rebuildRows();
    list.updateContent();
    int selectedRow = -1;
    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
        if (! rows[static_cast<size_t>(i)].header && rows[static_cast<size_t>(i)].scope == selectedScope && rows[static_cast<size_t>(i)].name == selectedName
            && rows[static_cast<size_t>(i)].isStruct == selectedIsStruct)
            selectedRow = i;
    if (selectedRow < 0)
        selectedName.clear();
    list.selectRow(selectedRow, true, true);
    editorView.setViewedComponent(nullptr, false);
    editor.reset();
    if (! selectedName.empty() && selectedIsStruct && findStruct(selectedScope, selectedName) != nullptr)
        editor = std::make_unique<StructEditor>(*this, selectedScope, selectedName);
    else if (! selectedName.empty() && ! selectedIsStruct && findEnum(selectedScope, selectedName) != nullptr)
        editor = std::make_unique<EnumEditor>(*this, selectedScope, selectedName);
    if (editor != nullptr)
        editorView.setViewedComponent(editor.get(), false);
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
    g.setColour(r.isStruct ? structBadge : enumBadge);
    g.fillRoundedRectangle(badge.toFloat(), 3.0f);
    g.setColour(juce::Colours::black);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText(r.isStruct ? "S" : "E", badge, juce::Justification::centred);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(13.0f));
    auto text = area.reduced(6, 0);
    g.drawText(r.title, text, juce::Justification::centredLeft, true);
    g.setColour(juce::Colour(0xff7f8ea3));
    g.setFont(juce::FontOptions(11.0f));
    if (r.isStruct)
    {
        if (const auto* def = findStruct(r.scope, r.name))
            g.drawText(juce::String(def->members.size()) + " members", text, juce::Justification::centredRight, true);
    }
    else if (const auto* def = findEnum(r.scope, r.name))
        g.drawText(juce::String(def->variants.size()) + " values", text, juce::Justification::centredRight, true);
}

void TypesPanel::selectedRowsChanged(int row)
{
    if (! juce::isPositiveAndBelow(row, static_cast<int>(rows.size())) || rows[static_cast<size_t>(row)].header)
        return;
    const auto& r = rows[static_cast<size_t>(row)];
    if (r.scope == selectedScope && r.name == selectedName && r.isStruct == selectedIsStruct)
        return;
    selectedScope = r.scope;
    selectedName = r.name;
    selectedIsStruct = r.isStruct;
    refreshSoon();
}

} // namespace creation::node_editor_ui
