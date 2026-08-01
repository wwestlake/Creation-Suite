#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <creation/services/SuiteContextEngine.h>

namespace creation::ui
{
class ChatTranscriptComponent;

// Suite-wide AI assistant chat surface -- generalized from Creation
// Station's original app-local AiPanel (same UI: mode/access selectors,
// multi-vendor provider/model combo already backed by
// creation::services::SuiteAiSettings/SuiteAiProviderRuntime, markdown
// chat transcript, hide/collapse). Any app can embed this; the engine
// wiring (creation::services::SuiteContextEngine, SuiteAiService) is the
// caller's responsibility, same pattern as CreationSuiteHeaderBar.
//
// Deliberately does NOT carry Creation Station's old TaskPlanner
// concept (multi-step task execution) -- that was app-specific, not a
// suite-wide chat UI concern. An app that still wants step-execution UI
// builds it as its own layer on top of this panel's callbacks.
class SuiteAiChatPanel final : public juce::Component,
                               private juce::TextEditor::Listener,
                               private juce::ComboBox::Listener
{
public:
    enum class GuidanceMode
    {
        normal,
        learn,
        research
    };

    enum class AccessLevel
    {
        askFirst,
        appOnly,
        fileChanges,
        fullAccess
    };

    SuiteAiChatPanel();
    ~SuiteAiChatPanel() override;

    void setGuidanceMode(GuidanceMode newMode);
    GuidanceMode getGuidanceMode() const noexcept { return guidanceMode; }

    void setAccessLevel(AccessLevel newLevel);
    AccessLevel getAccessLevel() const noexcept { return accessLevel; }

    void setAvailableModels(const juce::StringArray& modelIds, const juce::String& statusText);

    // Repopulates the provider combo from creation::services::
    // SuiteAiSettingsStore -- ONLY accounts that are enabled and usable
    // (have an API key, or belong to a provider that doesn't need one,
    // e.g. a local Ollama endpoint) ever appear. The user must configure
    // AI accounts in Suite Settings; this panel has no local provider/
    // key entry of its own. Call this at startup and again whenever
    // suite AI settings might have changed elsewhere (e.g. after the
    // user saves them in SuiteSettingsPanel).
    void RefreshConfiguredAccounts();

    void setSelectedAccountId(const juce::String& accountId);
    juce::String getSelectedAccountId() const;
    bool hasAnyConfiguredAccount() const noexcept { return ! configuredAccountIds_.isEmpty(); }

    void setSelectedModel(const juce::String& modelName);
    juce::String getSelectedModel() const;

    void setContextPacket(const creation::services::SuiteContextPacket& packet);

    // CTX-2: free-text steering note the user attaches to the NEXT
    // retrieval request -- the caller reads this via getSteeringNote()
    // when building a SuiteContextProcessInstruction, e.g.:
    //   request.processInstruction.steeringNote = panel.getSteeringNote();
    // This panel has no opinion on category boosts/excludes; a steering
    // note is the minimal, always-useful affordance every app needs,
    // more structured per-app instruction UI can layer on top later.
    juce::String getSteeringNote() const;
    void clearSteeringNote();

    void setAssistantResponse(const juce::String& responseText);
    void appendUserMessage(const juce::String& promptText);
    juce::String getPromptText() const;
    juce::String buildSubmissionPrompt() const;
    void setCollapsed(bool shouldCollapse);
    bool isCollapsed() const noexcept { return collapsed; }

    std::function<void(GuidanceMode mode)> onModeChanged;
    std::function<void(AccessLevel level)> onAccessChanged;
    std::function<void(const juce::String& accountId)> onAccountChanged;
    std::function<void(const juce::String& modelName)> onModelChanged;
    std::function<void(const juce::String& prompt)> onPromptSubmitted;
    std::function<void(bool shouldCollapse)> onCollapsedChanged;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    static juce::String modeLabel(GuidanceMode mode);
    static juce::String modeDescription(GuidanceMode mode);
    static juce::String modePromptPrefix(GuidanceMode mode);
    static juce::String accessLabel(AccessLevel level);
    static juce::String accessDescription(AccessLevel level);
    static juce::String accessPromptPrefix(AccessLevel level);

    void refreshModeUi();
    void refreshAccessUi();
    void refreshPromptHeight();
    void refreshChatLayout();
    void scrollChatToBottom();
    void textEditorTextChanged(juce::TextEditor& editor) override;
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;

    juce::Label headerLabel;
    juce::Label subtitleLabel;
    juce::Label modeLabelTitle;
    juce::TextButton normalModeButton { "Normal" };
    juce::TextButton learnModeButton { "Learn" };
    juce::TextButton researchModeButton { "Research" };
    juce::Label providerLabel;
    juce::ComboBox providerComboBox;
    juce::Label modelLabel;
    juce::ComboBox modelComboBox;
    juce::Label accessLabelTitle;
    juce::ComboBox accessComboBox;
    juce::Label steeringLabel;
    juce::TextEditor steeringNoteEditor;
    juce::Label promptLabel;
    juce::Viewport transcriptViewport;
    std::unique_ptr<ChatTranscriptComponent> transcriptContent;
    juce::TextEditor promptEditor;
    juce::TextButton sendButton { "Send" };
    juce::TextButton collapseButton { "Hide" };
    juce::Label footerHintLabel;
    GuidanceMode guidanceMode = GuidanceMode::normal;
    AccessLevel accessLevel = AccessLevel::askFirst;
    bool collapsed = false;
    int promptEditorHeight = 0;
    bool updatingComboBoxes = false;
    juce::StringArray availableModels;
    juce::String latestContextSummary;
    int pendingAssistantBubbleIndex = -1;

    // Parallel to providerComboBox's items (index i <-> combo id i+1) --
    // maps a combo selection back to the real SuiteAiSettings account id.
    // Empty when no usable account is configured (see hasAnyConfiguredAccount()).
    juce::StringArray configuredAccountIds_;
};
}
