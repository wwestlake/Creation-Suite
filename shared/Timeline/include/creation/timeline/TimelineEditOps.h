#pragma once

#include <vector>
#include "TimelineClip.h"

namespace creation::timeline
{
// Kind-agnostic clip edit operations, shared verbatim by every app that owns a clip/track vector
// pair (today: CreationStation's TimelineModel, which forwards its own moveClip/splitClip/
// deleteClip/duplicateClip member functions straight through to these). Operates on the caller's
// vectors directly rather than owning them, so it stays usable from any app-specific model shape.

bool moveClip(std::vector<TimelineClip>& clips, const std::vector<TimelineTrack>& tracks,
              int clipIndex, int trackIndex, double startSeconds);

bool duplicateClip(std::vector<TimelineClip>& clips, int clipIndex, double startOffsetSeconds = 0.25);

bool deleteClip(std::vector<TimelineClip>& clips, int clipIndex);

bool splitClip(std::vector<TimelineClip>& clips, int clipIndex, double splitSeconds);
}
