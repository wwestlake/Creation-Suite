#include <creation/ui/NamedAssetSaveLoadMenu.h>

namespace creation::ui
{
void showNamedAssetSaveLoadMenu(juce::Component& anchor,
                                const juce::String& itemLabel,
                                const juce::String& defaultName,
                                std::function<void(const juce::String&)> onSave,
                                std::function<void()> onLoad)
{
    juce::PopupMenu menu;
    menu.addItem(1, "Save...");
    menu.addItem(2, "Load...");

    auto area = anchor.getScreenBounds();
    // Captured once and checked before every use of the anchor (or anything owned by the same
    // component, like onSave/onLoad's captured `this`) - the menu and the name-entry dialog it
    // opens are both async, so the anchor's owning panel can be torn down (project close, panel
    // switch) while either is still on screen. A raw `&anchor` reference capture here would be a
    // use-after-free in that case; SafePointer degrades to null instead of dangling.
    auto anchorRef = juce::Component::SafePointer<juce::Component>(&anchor);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(area),
                       [anchorRef, itemLabel, defaultName, onSave, onLoad](int result)
                       {
                           if (anchorRef == nullptr)
                               return;

                           if (result == 1)
                           {
                               auto* prompt = new juce::AlertWindow("Save " + itemLabel,
                                                                    "Enter a name for this " + itemLabel.toLowerCase() + ":",
                                                                    juce::MessageBoxIconType::QuestionIcon);
                               prompt->addTextEditor("name", defaultName);
                               prompt->addButton("Save", 1);
                               prompt->addButton("Cancel", 0);

                               prompt->enterModalState(true, juce::ModalCallbackFunction::create([anchorRef, prompt, onSave](int promptResult) mutable
                               {
                                   std::unique_ptr<juce::AlertWindow> dialog(prompt);
                                   if (promptResult != 1 || anchorRef == nullptr)
                                       return;

                                   auto name = dialog->getTextEditorContents("name").trim();
                                   if (name.isEmpty())
                                       return;

                                   if (onSave)
                                       onSave(name);
                               }), true);
                           }
                           else if (result == 2)
                           {
                               if (onLoad)
                                   onLoad();
                           }
                       });
}
}
