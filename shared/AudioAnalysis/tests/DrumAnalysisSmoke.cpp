#include "creation/audio/DrumAnalysis.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace creation::audio;

static int failures = 0;
#define CHECK(cond) do { if (! (cond)) { std::printf("FAILED line %d: %s\n", __LINE__, #cond); ++failures; } } while (0)

int main()
{
    const double bpm = 120.0;   // a beat is 500 ms

    // Quantized eighth-note hats and a kick and snare: exactly on the grid.
    {
        std::vector<DrumHit> hits;
        for (int bar = 0; bar < 4; ++bar)
            for (int i = 0; i < 8; ++i)
                hits.push_back({ 42, 90, bar * 4 + i * 0.5, 0.25 });
        for (int bar = 0; bar < 4; ++bar)
        {
            hits.push_back({ 36, 110, bar * 4.0, 0.25 });
            hits.push_back({ 38, 100, bar * 4.0 + 1.0, 0.25 });
        }
        const auto r = analyzeDrums(hits, bpm);
        CHECK(r.found);
        CHECK(r.gridSubdivisions == 2);
        CHECK(r.meanAbsOffsetMs < 0.5);
        CHECK(r.lanes.size() == 3);
        CHECK(r.lanes.front().name == "closed hi-hat");
        CHECK(r.lanes.front().onGridExactly);
        std::printf("%s\n", describeDrums(r, "test track").c_str());
    }

    // A snare 12 ms behind the beat on every hit, with varied velocity: a laid-back lean.
    {
        std::vector<DrumHit> hits;
        for (int i = 0; i < 16; ++i)
            hits.push_back({ 38, 80 + (i % 4) * 10, i * 1.0 + 0.012 / 0.5, 0.25 });
        for (int i = 0; i < 16; ++i)
            hits.push_back({ 36, 100, i * 1.0, 0.25 });
        const auto r = analyzeDrums(hits, bpm);
        const DrumLane* snare = nullptr;
        for (const auto& l : r.lanes)
            if (l.pitch == 38) snare = &l;
        CHECK(snare != nullptr);
        CHECK(std::abs(snare->leanMs - 12.0) < 0.5);
        CHECK(snare->velocityDeviation > 5.0);
    }

    // Triplet feel: eighth-note triplets.
    {
        std::vector<DrumHit> hits;
        for (int i = 0; i < 24; ++i)
            hits.push_back({ 42, 90, i / 3.0, 0.1 });
        const auto r = analyzeDrums(hits, bpm);
        CHECK(r.gridSubdivisions == 3);
    }

    CHECK(drumName(36) == "kick");
    CHECK(drumName(99) == "note 99");
    CHECK(! analyzeDrums({}, bpm).found);

    if (failures == 0)
        std::printf("Drum analysis: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
