#include <creation/services/SuiteUndoService.h>

namespace creation::services
{
SuiteUndoContext::SuiteUndoContext(juce::String newContextId, int newMaxEntries)
    : contextId(std::move(newContextId)),
      maxEntries(juce::jmax(1, newMaxEntries))
{
}

const juce::String& SuiteUndoContext::getNextUndoLabel() const noexcept
{
    if (undoStack.empty())
        return emptyLabel;
    return undoStack.back().label;
}

const juce::String& SuiteUndoContext::getNextRedoLabel() const noexcept
{
    if (redoStack.empty())
        return emptyLabel;
    return redoStack.back().label;
}

void SuiteUndoContext::clear()
{
    undoStack.clear();
    redoStack.clear();
}

void SuiteUndoContext::pushUndoState(const juce::ValueTree& stateBeforeEdit, juce::String label)
{
    undoStack.push_back({ std::move(label), stateBeforeEdit.createCopy() });
    trimToLimit(undoStack);
    redoStack.clear();
}

bool SuiteUndoContext::undoTo(const juce::ValueTree& currentState, juce::ValueTree& stateToRestore, juce::String& labelOut)
{
    if (undoStack.empty())
        return false;

    auto entry = undoStack.back();
    undoStack.pop_back();
    redoStack.push_back({ entry.label, currentState.createCopy() });
    trimToLimit(redoStack);
    stateToRestore = entry.state.createCopy();
    labelOut = entry.label;
    return true;
}

bool SuiteUndoContext::redoTo(const juce::ValueTree& currentState, juce::ValueTree& stateToRestore, juce::String& labelOut)
{
    if (redoStack.empty())
        return false;

    auto entry = redoStack.back();
    redoStack.pop_back();
    undoStack.push_back({ entry.label, currentState.createCopy() });
    trimToLimit(undoStack);
    stateToRestore = entry.state.createCopy();
    labelOut = entry.label;
    return true;
}

juce::ValueTree SuiteUndoContext::serialise(const juce::String& rootType) const
{
    juce::ValueTree state(rootType);
    state.setProperty("contextId", contextId, nullptr);
    state.setProperty("maxEntries", maxEntries, nullptr);

    juce::ValueTree undoState("UndoStack");
    for (const auto& entry : undoStack)
    {
        auto entryState = entry.state.createCopy();
        entryState.setProperty("__undoLabel", entry.label, nullptr);
        undoState.addChild(entryState, -1, nullptr);
    }
    state.addChild(undoState, -1, nullptr);

    juce::ValueTree redoState("RedoStack");
    for (const auto& entry : redoStack)
    {
        auto entryState = entry.state.createCopy();
        entryState.setProperty("__undoLabel", entry.label, nullptr);
        redoState.addChild(entryState, -1, nullptr);
    }
    state.addChild(redoState, -1, nullptr);

    return state;
}

void SuiteUndoContext::restore(const juce::ValueTree& state)
{
    clear();
    if (! state.isValid())
        return;

    contextId = state.getProperty("contextId", contextId).toString();
    maxEntries = juce::jmax(1, (int) state.getProperty("maxEntries", maxEntries));

    auto loadStack = [](const juce::ValueTree& parent, std::vector<Entry>& target)
    {
        for (const auto child : parent)
        {
            auto restored = child.createCopy();
            auto label = restored.getProperty("__undoLabel").toString();
            restored.removeProperty("__undoLabel", nullptr);
            target.push_back({ label, restored });
        }
    };

    if (auto undoState = state.getChildWithName("UndoStack"); undoState.isValid())
        loadStack(undoState, undoStack);
    if (auto redoState = state.getChildWithName("RedoStack"); redoState.isValid())
        loadStack(redoState, redoStack);

    trimToLimit(undoStack);
    trimToLimit(redoStack);
}

void SuiteUndoContext::trimToLimit(std::vector<Entry>& stack)
{
    while ((int) stack.size() > maxEntries)
        stack.erase(stack.begin());
}

SuiteUndoContext& SuiteUndoService::getOrCreateContext(const juce::String& contextId, int maxEntries)
{
    if (auto* existing = findContext(contextId))
        return *existing;

    auto* created = contexts.add(new SuiteUndoContext(contextId, maxEntries));
    return *created;
}

SuiteUndoContext* SuiteUndoService::findContext(const juce::String& contextId)
{
    for (auto* context : contexts)
        if (context->getContextId() == contextId)
            return context;
    return nullptr;
}

const SuiteUndoContext* SuiteUndoService::findContext(const juce::String& contextId) const
{
    for (auto* context : contexts)
        if (context->getContextId() == contextId)
            return context;
    return nullptr;
}

void SuiteUndoService::setActiveContext(const juce::String& contextId)
{
    activeContextId = contextId;
}

SuiteUndoContext* SuiteUndoService::getActiveContext()
{
    return activeContextId.isNotEmpty() ? findContext(activeContextId) : nullptr;
}

const SuiteUndoContext* SuiteUndoService::getActiveContext() const
{
    return activeContextId.isNotEmpty() ? findContext(activeContextId) : nullptr;
}
}
