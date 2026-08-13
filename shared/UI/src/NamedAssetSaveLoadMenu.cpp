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
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(area),
                       [&anchor, itemLabel, defaultName, onSave, onLoad](int result)
                       {
                           if (result == 1)
                           {
                               auto* prompt = new juce::AlertWindow("Save " + itemLabel,
                                                                    "Enter a name for this " + itemLabel.toLowerCase() + ":",
                                                                    juce::MessageBoxIconType::QuestionIcon);
                               prompt->addTextEditor("name", defaultName);
                               prompt->addButton("Save", 1);
                               prompt->addButton("Cancel", 0);

                               auto options = juce::Component::SafePointer<juce::Component>(&anchor);
                               prompt->enterModalState(true, juce::ModalCallbackFunction::create([options, prompt, onSave](int promptResult) mutable
                               {
                                   std::unique_ptr<juce::AlertWindow> dialog(prompt);
                                   if (promptResult != 1 || options == nullptr)
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
