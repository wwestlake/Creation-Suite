#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <vector>

namespace creation::services
{
class SuiteUndoContext
{
public:
    struct Entry
    {
        juce::String label;
        juce::ValueTree state;
    };

    explicit SuiteUndoContext(juce::String contextId = {}, int maxEntries = 100);

    const juce::String& getContextId() const noexcept { return contextId; }
    int getUndoSize() const noexcept { return (int) undoStack.size(); }
    int getRedoSize() const noexcept { return (int) redoStack.size(); }
    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }
    const juce::String& getNextUndoLabel() const noexcept;
    const juce::String& getNextRedoLabel() const noexcept;

    void clear();
    void pushUndoState(const juce::ValueTree& stateBeforeEdit, juce::String label = {});
    bool undoTo(const juce::ValueTree& currentState, juce::ValueTree& stateToRestore, juce::String& labelOut);
    bool redoTo(const juce::ValueTree& currentState, juce::ValueTree& stateToRestore, juce::String& labelOut);

    juce::ValueTree serialise(const juce::String& rootType = "UndoContext") const;
    void restore(const juce::ValueTree& state);

private:
    void trimToLimit(std::vector<Entry>& stack);

    juce::String contextId;
    int maxEntries = 100;
    std::vector<Entry> undoStack;
    std::vector<Entry> redoStack;
    juce::String emptyLabel;
};

class SuiteUndoService
{
public:
    SuiteUndoContext& getOrCreateContext(const juce::String& contextId, int maxEntries = 100);
    SuiteUndoContext* findContext(const juce::String& contextId);
    const SuiteUndoContext* findContext(const juce::String& contextId) const;

    void setActiveContext(const juce::String& contextId);
    SuiteUndoContext* getActiveContext();
    const SuiteUndoContext* getActiveContext() const;
    const juce::String& getActiveContextId() const noexcept { return activeContextId; }

private:
    juce::OwnedArray<SuiteUndoContext> contexts;
    juce::String activeContextId;
};
}
