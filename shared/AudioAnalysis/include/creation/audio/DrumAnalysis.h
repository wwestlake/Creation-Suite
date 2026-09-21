#pragma once

#include <string>
#include <vector>

// Reading a MIDI drum track. Unlike audio, MIDI says exactly what was played and when, so nothing is guessed: every hit
// is grouped by drum (kick, snare, hi-hat...), and each drum's timing is compared with the beat grid and its velocity
// (how hard) is summarised. Timing variation on a MIDI drum track usually comes from Humanize, so the report describes
// the feel (tight, humanized, loose, laid back or pushed) and leaves the judgement about intent to the reader.
namespace creation::audio
{
struct DrumHit
{
    int pitch = 36;             // MIDI note number
    int velocity = 100;         // 1-127
    double beat = 0.0;          // position from the start of the timeline, in beats
    double lengthBeats = 0.0;
};

struct DrumLane
{
    int pitch = 0;
    std::string name;           // General MIDI drum name, or "note 47"
    int count = 0;
    double meanAbsOffsetMs = 0.0;   // average distance from the nearest grid line
    double spreadMs = 0.0;          // how much those distances vary
    double leanMs = 0.0;            // average signed distance: negative = ahead of the beat, positive = behind
    double maxAbsOffsetMs = 0.0;
    double meanVelocity = 0.0;
    int minVelocity = 0;
    int maxVelocity = 0;
    double velocityDeviation = 0.0; // standard deviation of velocity; near 0 is machine-like, 8 to 20 is played by hand
    bool onGridExactly = false;     // every hit within a millisecond: quantized and not humanized
};

struct DrumResult
{
    bool found = false;
    double bpm = 0.0;
    int gridSubdivisions = 0;       // steps to a beat on the grid used (2 eighths, 3 triplets, 4 sixteenths, 6 sextuplets)
    std::string feel;               // the same in words
    int totalHits = 0;
    double meanAbsOffsetMs = 0.0;   // over all drums
    std::vector<DrumLane> lanes;    // busiest first
};

// General MIDI percussion name for a note number (channel 10 map), or "note N".
std::string drumName(int pitch);

DrumResult analyzeDrums(const std::vector<DrumHit>& hits, double bpm, int subdivisionsPerBeat = 0);

std::string describeDrums(const DrumResult& result, const std::string& label);
}
