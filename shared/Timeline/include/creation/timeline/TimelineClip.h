#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <creation/assets/AssetTypes.h>
#include "TimelineTypes.h"

namespace creation::timeline
{
struct MidiNoteEvent
{
    juce::String id;
    int pitch = 60;            // MIDI note number, 0-127
    int velocity = 100;        // 1-127
    double startBeats = 0.0;   // relative to clip start
    double lengthBeats = 1.0;
    int channel = 1;           // MIDI channel, 1-16
    bool muted = false;
};

struct MidiCCEvent
{
    juce::String id;
    int controller = 1;        // CC number, 0-127
    int value = 0;             // 0-127
    double beats = 0.0;        // relative to clip start
};

// A single generic clip struct reused across every track kind, with kind-specific fields
// (midiNotes/midiCC/automationPoints/peaks) sitting unused for kinds that don't need them -
// carried over as-is rather than split into per-kind variants/polymorphism, since nothing in
// the suite needs that distinction yet and premature splitting would just add ceremony.
struct TimelineClip
{
    juce::String id;
    ClipKind kind = ClipKind::audio;
    juce::String displayName;
    juce::String assetId;
    creation::assets::AssetVersionId assetVersionId;
    creation::assets::AssetReferenceMode assetReferenceMode = creation::assets::AssetReferenceMode::exact;
    juce::String sourceTool;
    int trackIndex = -1;
    juce::File file;
    double startSeconds = 0.0;
    double durationSeconds = 0.0;
    double sourceStartSeconds = 0.0;
    double sourceDurationSeconds = 0.0;
    int sourceNumChannels = 0;
    bool recording = false;
    // Clips that share a non-empty linkGroupId move, trim, split, duplicate and delete together (a video and
    // its sound, Premiere-style). Empty = not linked.
    juce::String linkGroupId;
    // A video clip whose sound has been split onto its own audio clip: the picture no longer plays its own
    // sound, or it would be heard twice.
    bool soundDetached = false;
    // A video clip's own effect and layout settings (green screen, size/position/opacity for picture-in-picture...),
    // as named values so a new effect adds keys without changing the file format. Empty = all defaults.
    juce::NamedValueSet videoParams;
    std::vector<float> peaks;
    std::vector<float> rightPeaks;
    std::vector<MidiNoteEvent> midiNotes;
    std::vector<MidiCCEvent> midiCC;
    std::vector<AutomationPoint> automationPoints;
};
}
