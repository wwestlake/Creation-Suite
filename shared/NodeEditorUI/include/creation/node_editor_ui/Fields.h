#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace creation::node_editor_ui
{
// Suite rule for the node editors' panels (Types, Variables, Properties): clicking into a one-line field - a name, a
// value - selects its whole text, so typing replaces it; a second click places the cursor to edit part of it. Without
// this, typing went in next to the old text ("amount" + "paint Mode" -> "amountpaint Mode"). Multi-line fields
// (descriptions, scripts) keep ordinary clicking. Call it once a panel's fields are built.
inline void selectAllWhenFocused(juce::Component& root)
{
    for (auto* child : root.getChildren())
    {
        if (auto* field = dynamic_cast<juce::TextEditor*>(child); field != nullptr && ! field->isMultiLine())
            field->setSelectAllWhenFocused(true);
        selectAllWhenFocused(*child);
    }
}
} // namespace creation::node_editor_ui
