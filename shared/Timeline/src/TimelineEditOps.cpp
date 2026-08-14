#include <creation/timeline/TimelineEditOps.h>

namespace creation::timeline
{
bool moveClip(std::vector<TimelineClip>& clips, const std::vector<TimelineTrack>& tracks,
              int clipIndex, int trackIndex, double startSeconds)
{
    if (! juce::isPositiveAndBelow(clipIndex, (int) clips.size())
        || ! juce::isPositiveAndBelow(trackIndex, (int) tracks.size()))
        return false;

    auto& clip = clips[(size_t) clipIndex];
    if (clip.recording)
        return false;

    if (! canTrackContainClip(tracks[(size_t) trackIndex].kind, clip.kind))
        return false;

    clip.trackIndex = trackIndex;
    clip.startSeconds = juce::jmax(0.0, startSeconds);
    return true;
}

bool duplicateClip(std::vector<TimelineClip>& clips, int clipIndex, double startOffsetSeconds)
{
    if (! juce::isPositiveAndBelow(clipIndex, (int) clips.size()))
        return false;

    auto duplicate = clips[(size_t) clipIndex];
    if (duplicate.recording)
        return false;

    duplicate.id = juce::Uuid().toString();
    duplicate.displayName = duplicate.displayName.isNotEmpty() ? duplicate.displayName + " copy"
                                                               : duplicate.file.getFileNameWithoutExtension() + " copy";
    duplicate.startSeconds += juce::jmax(0.0, startOffsetSeconds);
    duplicate.recording = false;
    clips.push_back(std::move(duplicate));
    return true;
}

bool deleteClip(std::vector<TimelineClip>& clips, int clipIndex)
{
    if (! juce::isPositiveAndBelow(clipIndex, (int) clips.size()))
        return false;

    if (clips[(size_t) clipIndex].recording)
        return false;

    clips.erase(clips.begin() + clipIndex);
    return true;
}

bool splitClip(std::vector<TimelineClip>& clips, int clipIndex, double splitSeconds)
{
    if (! juce::isPositiveAndBelow(clipIndex, (int) clips.size()))
        return false;

    auto& clip = clips[(size_t) clipIndex];
    if (clip.recording)
        return false;

    constexpr auto minimumClipSeconds = 0.02;
    auto localSplitSeconds = splitSeconds - clip.startSeconds;
    if (localSplitSeconds <= minimumClipSeconds || localSplitSeconds >= clip.durationSeconds - minimumClipSeconds)
        return false;

    auto rightClip = clip;
    rightClip.id = juce::Uuid().toString();
    rightClip.startSeconds = splitSeconds;
    rightClip.sourceStartSeconds = clip.sourceStartSeconds + localSplitSeconds;
    rightClip.durationSeconds = clip.durationSeconds - localSplitSeconds;
    rightClip.displayName = clip.displayName.isNotEmpty() ? clip.displayName + " B"
                                                          : clip.file.getFileNameWithoutExtension() + " B";

    clip.durationSeconds = localSplitSeconds;
    if (clip.displayName.isNotEmpty())
        clip.displayName += " A";

    clips.insert(clips.begin() + clipIndex + 1, std::move(rightClip));
    return true;
}
}
