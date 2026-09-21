#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Finding the tempo and the beats of a recording, and how tightly the playing sits on them.
//
// It measures where the sound "attacks" (the onsets: a drum hit, a plucked or struck note, a consonant), finds the
// tempo that best explains their spacing, tracks the beats through the recording, and then compares each attack with
// the beat grid. Classical signal processing, no model to train, no dependencies. It works best on music with clear
// attacks (drums, guitar, piano, bass); a sustained pad or a smooth vocal has little to measure, and it says so
// through a low confidence rather than guessing.
namespace creation::audio
{
struct TempoOptions
{
    double minBpm = 50.0;
    double maxBpm = 200.0;
    double hopSeconds = 0.010;          // resolution of the onset measurement
    double expectedBpm = 0.0;           // if known (the project's tempo), used only to choose between double and half time
    double subdivisionsPerBeat = 0.0;   // the grid tightness is judged against; 0 = work it out (eighths, triplets or swing, sixteenths, sextuplets)
};

struct Onset
{
    double timeSeconds = 0.0;
    double strength = 0.0;              // relative, larger is a stronger attack
};

struct TempoResult
{
    bool found = false;
    double bpm = 0.0;
    double confidence = 0.0;            // 0..1; low means the rhythm is unclear or there are few attacks
    double beatSeconds = 0.0;           // the spacing of beats, 60 / bpm
    std::vector<double> beats;          // beat times in seconds
    std::vector<Onset> onsets;

    // Steadiness: how even the beat spacing is.
    double beatJitterMs = 0.0;          // standard deviation of the spacing between beats
    double driftBpm = 0.0;              // tempo of the last half minus the first half (positive: speeding up)

    // Tightness: how far the attacks are from the beat grid (beats and their subdivisions), on the grid the playing uses.
    int gridSubdivisions = 0;           // steps to a beat on that grid (2 eighths, 3 triplets or swing, 4 sixteenths, 6 sextuplets)
    std::string feel;                   // the same in words
    std::vector<std::string> pattern;   // the rhythm bar by bar: X strong attack, x lighter attack, . none; four beats to a bar
    size_t onsetsMeasured = 0;
    double meanAbsOffsetMs = 0.0;       // average distance from the nearest grid line
    double spreadMs = 0.0;              // how much those distances vary
    double leanMs = 0.0;                // average signed distance: negative = ahead of the beat (rushing), positive = behind (dragging)
};

TempoResult analyzeTempo(const float* samples, size_t count, double sampleRate, const TempoOptions& options = {});

// A short report a person or an assistant can read. `label` names what was measured; `projectBpm` (0 = unknown) is the
// tempo the project is set to, compared with what was found.
std::string describeTempo(const TempoResult& result, const std::string& label, double startSeconds, double endSeconds,
                          double projectBpm = 0.0);
}
