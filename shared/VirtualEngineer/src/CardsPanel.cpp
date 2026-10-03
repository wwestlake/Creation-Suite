#include <creation/agent/CardsPanel.h>

namespace creation::agent
{
namespace ls = creation::litesemrag;

namespace
{
const juce::Colour background { 0xff15181d };
const juce::Colour textColour { 0xffd6dde8 };
const juce::Colour quietColour { 0xff7f8ea3 };

juce::Colour scopeColour(ls::CardScope scope)
{
    switch (scope)
    {
        case ls::CardScope::shipped: return juce::Colour(0xff7f8ea3);
        case ls::CardScope::suite: return juce::Colour(0xff5a9ce0);
        case ls::CardScope::app: return juce::Colour(0xff6cc46a);
        case ls::CardScope::project: return juce::Colour(0xffe8c84a);
    }
    return quietColour;
}

juce::StringArray linesOf(const juce::String& text)
{
    juce::StringArray lines;
    lines.addLines(text);
    lines.trim();
    lines.removeEmptyStrings();
    return lines;
}

// Clicking a one-line field selects it, so typing replaces it (as in every suite panel).
void selectAllWhenFocused(juce::Component& root)
{
    for (auto* child : root.getChildren())
    {
        if (auto* field = dynamic_cast<juce::TextEditor*>(child); field != nullptr && ! field->isMultiLine())
            field->setSelectAllWhenFocused(true);
        selectAllWhenFocused(*child);
    }
}
} // namespace

//==============================================================================
// One card's fields. Process fields (when, steps, never, evidence, ask when) take one item per line.
class CardsPanel::Editor final : public juce::Component
{
public:
    Editor()
    {
        for (const auto& kind : ls::cardKinds())
            kindBox.addItem(kind, kindBox.getNumItems() + 1);
        priority.setRange(0, 100, 1);
        priority.setSliderStyle(juce::Slider::LinearHorizontal);
        priority.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40, 20);
        active.setButtonText("Active");
        for (auto* multi : { &text, &steps, &gates, &evidence, &escalation })
        {
            multi->setMultiLine(true, true);
            multi->setReturnKeyStartsNewLine(true);
        }
        text.setTextToShowWhenEmpty("The guidance itself, in words the engineer follows", quietColour);
        tokens.setTextToShowWhenEmpty("words that bring it up, comma separated", quietColour);
        trigger.setTextToShowWhenEmpty("process cards: when it applies", quietColour);
        steps.setTextToShowWhenEmpty("one step per line", quietColour);
        gates.setTextToShowWhenEmpty("one per line: what must not happen", quietColour);
        evidence.setTextToShowWhenEmpty("one per line: what proves it was followed", quietColour);
        escalation.setTextToShowWhenEmpty("one per line: when to stop and ask", quietColour);
        add("Id", id);
        add("Kind", kindBox);
        add("Title", titleField);
        add("Priority", priority);
        add("", active);
        add("Tokens", tokens);
        add("Text", text, 110);
        add("When", trigger);
        add("Steps", steps, 70);
        add("Never", gates, 50);
        add("Evidence", evidence, 50);
        add("Ask when", escalation, 50);
    }

    void show(const ls::Card& card, bool editable)
    {
        id.setText(card.id, false);
        kindBox.setText(card.kind.isEmpty() ? juce::String("rule") : card.kind, juce::dontSendNotification);
        titleField.setText(card.title, false);
        priority.setValue(card.priority, juce::dontSendNotification);
        active.setToggleState(card.isActive(), juce::dontSendNotification);
        tokens.setText(card.tokens.joinIntoString(", "), false);
        text.setText(card.text, false);
        trigger.setText(card.trigger, false);
        steps.setText(card.steps.joinIntoString("\n"), false);
        gates.setText(card.gates.joinIntoString("\n"), false);
        evidence.setText(card.evidence.joinIntoString("\n"), false);
        escalation.setText(card.escalation.joinIntoString("\n"), false);
        for (auto* field : { &id, &titleField, &tokens, &text, &trigger, &steps, &gates, &evidence, &escalation })
            field->setReadOnly(! editable);
        kindBox.setEnabled(editable);
        priority.setEnabled(editable);
        active.setEnabled(editable);
    }

    ls::Card read(const juce::String& source) const
    {
        ls::Card card;
        card.id = id.getText().trim();
        card.kind = kindBox.getText().trim();
        card.title = titleField.getText().trim();
        card.priority = static_cast<int>(priority.getValue());
        card.status = active.getToggleState() ? "active" : "retired";
        juce::StringArray words;
        words.addTokens(tokens.getText(), ",", "");
        words.trim();
        words.removeEmptyStrings();
        card.tokens = words;
        card.text = text.getText().trim();
        card.trigger = trigger.getText().trim();
        card.steps = linesOf(steps.getText());
        card.gates = linesOf(gates.getText());
        card.evidence = linesOf(evidence.getText());
        card.escalation = linesOf(escalation.getText());
        card.source = source;
        return card;
    }

    int preferredHeight() const
    {
        int h = 0;
        for (const auto& row : rows)
            h += row.height + 4;
        return h;
    }

    void resized() override
    {
        int y = 0;
        for (auto& row : rows)
        {
            auto area = juce::Rectangle<int>(0, y, getWidth(), row.height);
            row.label->setBounds(area.removeFromLeft(70));
            row.component->setBounds(area);
            y += row.height + 4;
        }
    }

private:
    struct FieldRow
    {
        std::unique_ptr<juce::Label> label;
        juce::Component* component;
        int height;
    };

    void add(const juce::String& name, juce::Component& component, int height = 24)
    {
        auto label = std::make_unique<juce::Label>();
        label->setText(name, juce::dontSendNotification);
        label->setColour(juce::Label::textColourId, quietColour);
        addAndMakeVisible(*label);
        addAndMakeVisible(component);
        rows.push_back({ std::move(label), &component, height });
    }

    std::vector<FieldRow> rows;

public:
    juce::TextEditor id, titleField, tokens, text, trigger, steps, gates, evidence, escalation;
    juce::ComboBox kindBox;
    juce::Slider priority;
    juce::ToggleButton active;
};

//==============================================================================
CardsPanel::CardsPanel(VirtualEngineer& e) : engineer(e), editor(std::make_unique<Editor>())
{
    title.setText("Cards", juce::dontSendNotification);
    title.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    newButton.onClick = [this] { showNewMenu(); };
    saveButton.onClick = [this] { save(); };
    retireButton.onClick = [this] { retireOrDelete(false); };
    deleteButton.onClick = [this] { retireOrDelete(true); };
    retireButton.setTooltip("Keep the card, but stop it applying (in a closer scope, this also hides the same card farther out)");
    for (auto* b : { &newButton, &saveButton, &retireButton, &deleteButton })
        addAndMakeVisible(*b);

    list.setModel(this);
    list.setRowHeight(24);
    list.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff11141a));
    addAndMakeVisible(list);

    editorView.setViewedComponent(editor.get(), false);
    editorView.setScrollBarsShown(true, false);
    addAndMakeVisible(editorView);

    status.setColour(juce::Label::textColourId, juce::Colour(0xffff8a80));
    addAndMakeVisible(status);

    tryLabel.setText("Try a request - which cards would it bring up?", juce::dontSendNotification);
    tryLabel.setColour(juce::Label::textColourId, quietColour);
    addAndMakeVisible(tryLabel);
    tryRequestText.onReturnKey = [this] { tryRequest(); };
    addAndMakeVisible(tryRequestText);
    tryButton.onClick = [this] { tryRequest(); };
    addAndMakeVisible(tryButton);
    tryResult.setColour(juce::Label::textColourId, textColour);
    tryResult.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(tryResult);

    selectAllWhenFocused(*this);
    refresh();
}

CardsPanel::~CardsPanel()
{
    list.setModel(nullptr);
}

ls::CardStore::Place CardsPanel::placeFor(ls::CardScope scope) const
{
    if (scope == ls::CardScope::app)
        return ls::CardStore::forApp(engineer.getApp());
    if (scope == ls::CardScope::project)
        return ls::CardStore::forProject(engineer.getProjectId());
    if (scope == ls::CardScope::shipped)
        return { ls::CardScope::shipped };
    return ls::CardStore::suite();
}

void CardsPanel::refresh()
{
    const auto keepId = selectedRow >= 0 && selectedRow < static_cast<int>(rows.size()) ? rows[static_cast<size_t>(selectedRow)].card.id : juce::String();
    const auto keepScope = selectedRow >= 0 && selectedRow < static_cast<int>(rows.size()) ? rows[static_cast<size_t>(selectedRow)].scope : ls::CardScope::suite;
    juce::String error;
    rows.clear();
    for (const auto& scoped : engineer.loadCards(error))
        for (const auto& card : scoped.cards)
            rows.push_back({ scoped.scope, card });
    // Closest scope first, then by title.
    std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        if (a.scope != b.scope)
            return static_cast<int>(a.scope) > static_cast<int>(b.scope);
        return a.card.title.compareIgnoreCase(b.card.title) < 0;
    });
    list.updateContent();
    problem(error);
    selectedRow = -1;
    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
        if (rows[static_cast<size_t>(i)].card.id == keepId && rows[static_cast<size_t>(i)].scope == keepScope)
            selectedRow = i;
    if (selectedRow >= 0)
        list.selectRow(selectedRow, false, true);
    select(selectedRow);
}

int CardsPanel::getNumRows()
{
    return static_cast<int>(rows.size());
}

void CardsPanel::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || row >= static_cast<int>(rows.size()))
        return;
    const auto& r = rows[static_cast<size_t>(row)];
    if (selected)
        g.fillAll(juce::Colour(0xff2a4a6d));
    auto area = juce::Rectangle<int>(0, 0, width, height).reduced(6, 3);
    const auto badge = area.removeFromLeft(56);
    g.setColour(scopeColour(r.scope).withAlpha(0.85f));
    g.fillRoundedRectangle(badge.toFloat(), 3.0f);
    g.setColour(juce::Colours::black);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText(ls::scopeName(r.scope), badge, juce::Justification::centred);
    area.removeFromLeft(6);
    g.setColour(r.card.isActive() ? textColour : quietColour);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText((r.card.kind.isEmpty() ? juce::String() : r.card.kind + " - ") + r.card.title + (r.card.isActive() ? juce::String() : "  (retired)"),
               area, juce::Justification::centredLeft, true);
}

void CardsPanel::selectedRowsChanged(int row)
{
    select(row);
}

void CardsPanel::select(int row)
{
    editingNew = false;
    selectedRow = row;
    if (row < 0 || row >= static_cast<int>(rows.size()))
    {
        editor->show({}, false);
        for (auto* b : { &saveButton, &retireButton, &deleteButton })
            b->setEnabled(false);
    }
    else
    {
        const auto& r = rows[static_cast<size_t>(row)];
        const bool editable = r.scope != ls::CardScope::shipped;
        editor->show(r.card, editable);
        editingOriginalId = r.card.id;
        saveButton.setEnabled(editable);
        retireButton.setEnabled(editable && r.card.isActive());
        deleteButton.setEnabled(editable);
    }
    resized();
}

void CardsPanel::showNewMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "For every app and project (suite)");
    menu.addItem(2, "For this app only");
    menu.addItem(3, "For this project only", engineer.getProjectId().isNotEmpty());
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&newButton), [safe = juce::Component::SafePointer<CardsPanel>(this)](int chosen) {
        if (safe == nullptr || chosen <= 0)
            return;
        safe->startNew(chosen == 1 ? ls::CardScope::suite : chosen == 2 ? ls::CardScope::app : ls::CardScope::project);
    });
}

void CardsPanel::startNew(ls::CardScope scope)
{
    list.deselectAllRows();
    selectedRow = -1;
    editingNew = true;
    newScope = scope;
    editingOriginalId.clear();
    ls::Card card;
    card.kind = "rule";
    card.id = ls::scopeName(scope) + ".rule.";
    editor->show(card, true);
    saveButton.setEnabled(true);
    retireButton.setEnabled(false);
    deleteButton.setEnabled(false);
    problem("New " + ls::scopeName(scope) + " card: fill it in and Save.", true);
    resized();
}

void CardsPanel::save()
{
    const auto scope = editingNew ? newScope : (selectedRow >= 0 && selectedRow < static_cast<int>(rows.size()) ? rows[static_cast<size_t>(selectedRow)].scope : newScope);
    if (scope == ls::CardScope::shipped)
        return;
    const auto card = editor->read("user");
    const auto place = placeFor(scope);
    juce::String error;
    // A renamed card replaces its old id rather than leaving a copy behind.
    if (! editingNew && editingOriginalId.isNotEmpty() && editingOriginalId != card.id)
        ls::CardStore::remove(place, editingOriginalId, error);
    error.clear();
    if (! ls::CardStore::upsert(place, card, error))
    {
        problem(error);
        return;
    }
    editingNew = false;
    // Select the saved card again after the reload.
    refresh();
    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
        if (rows[static_cast<size_t>(i)].card.id == card.id && rows[static_cast<size_t>(i)].scope == scope)
        {
            list.selectRow(i);
            break;
        }
    problem("Saved.", true);
}

void CardsPanel::retireOrDelete(bool deleteIt)
{
    if (selectedRow < 0 || selectedRow >= static_cast<int>(rows.size()))
        return;
    const auto row = rows[static_cast<size_t>(selectedRow)];
    if (row.scope == ls::CardScope::shipped)
        return;
    juce::String error;
    bool ok = false;
    if (deleteIt)
        ok = ls::CardStore::remove(placeFor(row.scope), row.card.id, error);
    else
    {
        auto retired = row.card;
        retired.status = "retired";
        ok = ls::CardStore::upsert(placeFor(row.scope), retired, error);
    }
    if (! ok)
    {
        problem(error);
        return;
    }
    refresh();
    problem(deleteIt ? "Deleted." : "Retired - it no longer applies.", true);
}

void CardsPanel::tryRequest()
{
    juce::String error;
    const auto retrieval = engineer.retrieveFor(tryRequestText.getText(), error);
    juce::String shown;
    shown << "Matched on: " << (retrieval.tokens.isEmpty() ? juce::String("(no words long enough)") : retrieval.tokens.joinIntoString(", ")) << "\n";
    if (retrieval.cards.empty())
        shown << "No card applies.";
    for (size_t i = 0; i < retrieval.cards.size(); ++i)
        shown << (i + 1) << ". [" << ls::scopeName(retrieval.cards[i].scope) << "] " << retrieval.cards[i].card.title << "  (priority "
              << retrieval.cards[i].card.priority << ")\n";
    tryResult.setText(shown, juce::dontSendNotification);
    problem(error);
}

void CardsPanel::problem(const juce::String& text, bool good)
{
    status.setColour(juce::Label::textColourId, good ? juce::Colour(0xff6cc46a) : juce::Colour(0xffff8a80));
    status.setText(text, juce::dontSendNotification);
}

void CardsPanel::paint(juce::Graphics& g)
{
    g.fillAll(background);
}

void CardsPanel::resized()
{
    auto area = getLocalBounds().reduced(8);
    auto top = area.removeFromTop(26);
    title.setBounds(top.removeFromLeft(70));
    newButton.setBounds(top.removeFromRight(100));
    area.removeFromTop(6);
    list.setBounds(area.removeFromTop(juce::jlimit(80, 260, area.getHeight() / 3)));
    area.removeFromTop(6);

    auto tryArea = area.removeFromBottom(130);
    tryLabel.setBounds(tryArea.removeFromTop(20));
    auto tryRow = tryArea.removeFromTop(26);
    tryButton.setBounds(tryRow.removeFromRight(50));
    tryRow.removeFromRight(4);
    tryRequestText.setBounds(tryRow);
    tryResult.setBounds(tryArea.reduced(0, 4));

    status.setBounds(area.removeFromBottom(20));
    auto buttons = area.removeFromBottom(28);
    saveButton.setBounds(buttons.removeFromLeft(70));
    buttons.removeFromLeft(4);
    retireButton.setBounds(buttons.removeFromLeft(70));
    buttons.removeFromLeft(4);
    deleteButton.setBounds(buttons.removeFromLeft(70));
    area.removeFromBottom(4);
    editorView.setBounds(area);
    editor->setSize(juce::jmax(100, area.getWidth() - editorView.getScrollBarThickness()), editor->preferredHeight());
}
} // namespace creation::agent
