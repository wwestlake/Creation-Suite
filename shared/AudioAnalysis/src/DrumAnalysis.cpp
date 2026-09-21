#include "creation/audio/DrumAnalysis.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

namespace creation::audio
{
namespace
{
// Signed distance in ms from a hit to the nearest line of a grid of `steps` per beat.
double offsetMs(double beat, int steps, double bpm)
{
    const double position = beat * steps;
    const double nearest = std::round(position);
    return (position - nearest) / steps * 60000.0 / bpm;
}

double meanAbsOver(const std::vector<DrumHit>& hits, int steps, double bpm)
{
    double sum = 0.0;
    for (const auto& h : hits)
        sum += std::abs(offsetMs(h.beat, steps, bpm));
    return hits.empty() ? 0.0 : sum / (double) hits.size();
}

const char* feelWords(int steps)
{
    switch (steps)
    {
        case 2: return "eighth notes";
        case 3: return "triplets (or swung eighths)";
        case 4: return "sixteenth notes";
        case 6: return "sextuplets";
        default: return "the beat";
    }
}
}

std::string drumName(int pitch)
{
    static const std::map<int, const char*> names = {
        { 35, "acoustic bass drum" }, { 36, "kick" }, { 37, "side stick" }, { 38, "snare" }, { 39, "hand clap" },
        { 40, "electric snare" }, { 41, "low floor tom" }, { 42, "closed hi-hat" }, { 43, "high floor tom" },
        { 44, "pedal hi-hat" }, { 45, "low tom" }, { 46, "open hi-hat" }, { 47, "low-mid tom" }, { 48, "high-mid tom" },
        { 49, "crash cymbal" }, { 50, "high tom" }, { 51, "ride cymbal" }, { 52, "china cymbal" }, { 53, "ride bell" },
        { 54, "tambourine" }, { 55, "splash cymbal" }, { 56, "cowbell" }, { 57, "crash cymbal 2" }, { 59, "ride cymbal 2" },
        { 60, "high bongo" }, { 61, "low bongo" }, { 62, "mute high conga" }, { 63, "open high conga" }, { 64, "low conga" },
        { 69, "cabasa" }, { 70, "maracas" }, { 75, "claves" }, { 76, "high wood block" }, { 77, "low wood block" } };
    const auto found = names.find(pitch);
    return found != names.end() ? found->second : "note " + std::to_string(pitch);
}

DrumResult analyzeDrums(const std::vector<DrumHit>& hits, double bpm, int subdivisionsPerBeat)
{
    DrumResult result;
    result.bpm = bpm;
    if (hits.empty() || bpm <= 0.0)
        return result;
    result.found = true;
    result.totalHits = (int) hits.size();

    // The grid the playing uses: the coarsest of eighths, triplets, sixteenths, sextuplets that the hits sit close to
    // (within 8 ms, or an eighth of a step); if none do, the closest of them.
    int steps = subdivisionsPerBeat;
    if (steps <= 0)
    {
        static const int candidates[] = { 2, 3, 4, 6 };
        double best = 1e9;
        int bestSteps = 4;
        steps = 0;
        for (int c : candidates)
        {
            const double mean = meanAbsOver(hits, c, bpm);
            const double step = 60000.0 / bpm / c;
            if (steps == 0 && mean <= std::max(8.0, step / 8.0) && c != 3)
                steps = c;
            if (mean < best - 1e-9 && c != 6)
            {
                best = mean;
                bestSteps = c;
            }
        }
        // Triplets only win when they fit clearly better than sixteenths (which they cannot both fit).
        if (meanAbsOver(hits, 3, bpm) <= 8.0 && meanAbsOver(hits, 4, bpm) > 12.0 && meanAbsOver(hits, 2, bpm) > 12.0)
            steps = 3;
        if (steps == 0)
            steps = bestSteps;
    }
    result.gridSubdivisions = steps;
    result.feel = feelWords(steps);

    struct Accum
    {
        std::vector<double> offsets;
        std::vector<int> velocities;
    };
    std::map<int, Accum> perLane;
    double sumAbs = 0.0;
    for (const auto& h : hits)
    {
        const double off = offsetMs(h.beat, steps, bpm);
        perLane[h.pitch].offsets.push_back(off);
        perLane[h.pitch].velocities.push_back(h.velocity);
        sumAbs += std::abs(off);
    }
    result.meanAbsOffsetMs = sumAbs / (double) hits.size();

    for (const auto& [pitch, a] : perLane)
    {
        DrumLane lane;
        lane.pitch = pitch;
        lane.name = drumName(pitch);
        lane.count = (int) a.offsets.size();
        double sum = 0.0, sumAbsLane = 0.0, maxAbs = 0.0;
        for (double o : a.offsets)
        {
            sum += o;
            sumAbsLane += std::abs(o);
            maxAbs = std::max(maxAbs, std::abs(o));
        }
        const double n = (double) lane.count;
        lane.leanMs = sum / n;
        lane.meanAbsOffsetMs = sumAbsLane / n;
        lane.maxAbsOffsetMs = maxAbs;
        double var = 0.0;
        for (double o : a.offsets)
            var += (o - lane.leanMs) * (o - lane.leanMs);
        lane.spreadMs = std::sqrt(var / n);
        lane.onGridExactly = maxAbs < 1.0;

        double vsum = 0.0;
        lane.minVelocity = 127;
        lane.maxVelocity = 0;
        for (int v : a.velocities)
        {
            vsum += v;
            lane.minVelocity = std::min(lane.minVelocity, v);
            lane.maxVelocity = std::max(lane.maxVelocity, v);
        }
        lane.meanVelocity = vsum / n;
        double vvar = 0.0;
        for (int v : a.velocities)
            vvar += (v - lane.meanVelocity) * (v - lane.meanVelocity);
        lane.velocityDeviation = std::sqrt(vvar / n);
        result.lanes.push_back(lane);
    }
    std::sort(result.lanes.begin(), result.lanes.end(), [](const DrumLane& a, const DrumLane& b) { return a.count > b.count; });
    return result;
}

std::string describeDrums(const DrumResult& result, const std::string& label)
{
    if (! result.found)
        return "No MIDI drum notes were found on " + label + ", or the project tempo is unknown.\n";

    char line[320];
    std::string report;
    std::snprintf(line, sizeof(line), "MIDI drum analysis of %s: %d hits at %.1f BPM, measured against a grid of %s.\n", label.c_str(),
                  result.totalHits, result.bpm, result.feel.c_str());
    report += line;

    const char* overall = result.meanAbsOffsetMs < 1.0 ? "exactly on the grid (quantized, no humanization)"
                          : result.meanAbsOffsetMs < 6.0 ? "very tight, a light humanization or a precise player"
                          : result.meanAbsOffsetMs < 15.0 ? "a natural human feel, consistent with humanization"
                          : result.meanAbsOffsetMs < 30.0 ? "loose"
                                                          : "very loose, or the notes are on a different grid than the one measured";
    std::snprintf(line, sizeof(line), "Overall the hits average %.1f ms from the grid: %s.\n", result.meanAbsOffsetMs, overall);
    report += line;
    report += "Each drum: hits, average ms from the grid, lean (- ahead of the beat, + behind), spread, and velocity (average, range, variation).\n";

    for (const auto& lane : result.lanes)
    {
        std::snprintf(line, sizeof(line), "  %-20s %4d hits  %4.1f ms off  lean %+5.1f ms (%s)  spread %.1f ms  velocity %.0f (%d-%d, +/-%.0f) %s\n",
                      lane.name.c_str(), lane.count, lane.meanAbsOffsetMs, lane.leanMs,
                      lane.leanMs > 3.0 ? "behind" : lane.leanMs < -3.0 ? "ahead" : "centred", lane.spreadMs, lane.meanVelocity,
                      lane.minVelocity, lane.maxVelocity, lane.velocityDeviation,
                      lane.onGridExactly ? "[exactly on the grid]" : lane.velocityDeviation < 1.5 && lane.count > 3 ? "[same velocity every hit]" : "");
        report += line;
    }
    report += "These are exact positions read from the MIDI, not estimates. A steady lean on one drum (snare behind, hats ahead) is a "
              "deliberate feel; velocity variation that follows a pattern (accents, ghost notes) is a groove; the same velocity and "
              "position on every hit is a programmed, unhumanized part.\n";
    return report;
}
}
