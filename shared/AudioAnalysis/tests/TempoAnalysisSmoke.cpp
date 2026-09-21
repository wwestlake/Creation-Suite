// Checks the tempo and beat analyzer against click tracks whose tempo and timing are known exactly.

#include <creation/audio/TempoAnalysis.h>

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
int failures = 0;

void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "PASS  " : "FAIL  ") << what << std::endl;
    if (! ok)
        ++failures;
}

constexpr double pi = 3.14159265358979323846;
constexpr double rate = 44100.0;

// A short percussive hit (a decaying burst of a tone and some noise) at `time`.
void addHit(std::vector<float>& audio, double time, double amplitude = 0.8)
{
    const size_t start = (size_t) (time * rate);
    const size_t length = (size_t) (0.06 * rate);
    static unsigned int seed = 7;
    for (size_t i = 0; i < length && start + i < audio.size(); ++i)
    {
        const double t = (double) i / rate;
        seed = seed * 1664525u + 1013904223u;
        const double noise = (double) (seed >> 8) / (double) (1u << 24) * 2.0 - 1.0;
        const double envelope = std::exp(-t * 70.0);
        audio[start + i] += (float) (amplitude * envelope * (0.6 * std::sin(2.0 * pi * 180.0 * t) + 0.4 * noise));
    }
}

// Beats every `beat` seconds with an off-beat eighth note between them, each hit moved by `jitter(k)` seconds.
template <typename Jitter>
std::vector<float> makeGroove(double bpm, double seconds, Jitter jitter)
{
    std::vector<float> audio((size_t) (seconds * rate), 0.0f);
    const double beat = 60.0 / bpm;
    int k = 0;
    for (double t = 0.3; t < seconds - 0.2; t += beat / 2.0, ++k)
        addHit(audio, t + jitter(k), k % 2 == 0 ? 0.9 : 0.5);   // strong on the beat, softer between
    return audio;
}
}

int main()
{
    using namespace creation::audio;

    // ---- A steady groove at 100 BPM ----
    {
        const auto audio = makeGroove(100.0, 16.0, [](int) { return 0.0; });
        const auto result = analyzeTempo(audio.data(), audio.size(), rate);
        check(result.found, "a tempo is found in a steady groove");
        check(std::abs(result.bpm - 100.0) < 1.5, "100 BPM measured as " + std::to_string(result.bpm));
        check(result.confidence > 0.3, "and it is confident: " + std::to_string(result.confidence));
        check(result.beats.size() >= 20 && result.beats.size() <= 30, "about 26 beats are tracked (" + std::to_string(result.beats.size()) + ")");
        check(result.beatJitterMs < 15.0, "the steady groove has little beat-to-beat wobble: " + std::to_string(result.beatJitterMs) + " ms");
        check(std::abs(result.driftBpm) < 2.0, "and no drift: " + std::to_string(result.driftBpm));
        check(result.meanAbsOffsetMs < 15.0, "its attacks sit tight to the grid: " + std::to_string(result.meanAbsOffsetMs) + " ms");
    }

    // ---- Another tempo, and one near the edges ----
    for (double bpm : { 76.0, 128.0, 160.0 })
    {
        const auto audio = makeGroove(bpm, 16.0, [](int) { return 0.0; });
        const auto result = analyzeTempo(audio.data(), audio.size(), rate);
        // Half or double time is a legitimate reading of a groove with off-beats; either is acceptable, exactly is best.
        const bool exact = std::abs(result.bpm - bpm) < 2.0;
        const bool octave = std::abs(result.bpm - 2.0 * bpm) < 3.0 || std::abs(result.bpm - 0.5 * bpm) < 2.0;
        check(result.found && (exact || octave), std::to_string((int) bpm) + " BPM measured as " + std::to_string(result.bpm)
                                                      + (exact ? " (exact)" : octave ? " (half or double time)" : ""));
    }

    // ---- A loose player is measured as looser than a tight one ----
    {
        unsigned int seed = 99;
        auto loose = [&](int) {
            seed = seed * 1664525u + 1013904223u;
            return ((double) (seed >> 8) / (double) (1u << 24) * 2.0 - 1.0) * 0.035;   // up to 35 ms either way
        };
        const auto tightAudio = makeGroove(100.0, 16.0, [](int) { return 0.0; });
        const auto looseAudio = makeGroove(100.0, 16.0, loose);
        const auto tight = analyzeTempo(tightAudio.data(), tightAudio.size(), rate);
        const auto sloppy = analyzeTempo(looseAudio.data(), looseAudio.size(), rate);
        check(sloppy.found && std::abs(sloppy.bpm - 100.0) < 3.0, "the loose player's tempo is still found: " + std::to_string(sloppy.bpm));
        check(sloppy.meanAbsOffsetMs > tight.meanAbsOffsetMs + 4.0,
              "the loose groove is measured as looser: " + std::to_string(sloppy.meanAbsOffsetMs) + " ms against " + std::to_string(tight.meanAbsOffsetMs));
    }

    // ---- A groove that speeds up ----
    {
        std::vector<float> audio((size_t) (20.0 * rate), 0.0f);
        double time = 0.3;
        double bpm = 90.0;
        while (time < 19.6)
        {
            addHit(audio, time, 0.9);
            time += 60.0 / bpm;
            bpm += 0.9;   // about 6 BPM faster over 20 seconds
        }
        const auto result = analyzeTempo(audio.data(), audio.size(), rate);
        check(result.found && result.driftBpm > 2.0, "a speeding-up groove is reported as speeding up: " + std::to_string(result.driftBpm));
    }

    // ---- Silence and too-short audio ----
    {
        std::vector<float> silence((size_t) (10.0 * rate), 0.0f);
        const auto quiet = analyzeTempo(silence.data(), silence.size(), rate);
        check(! quiet.found || quiet.confidence < 0.3, "silence has no confident tempo");
        std::vector<float> tiny((size_t) (1.0 * rate), 0.0f);
        check(! analyzeTempo(tiny.data(), tiny.size(), rate).found, "one second is too short to judge");
    }

    // ---- The report ----
    {
        const auto audio = makeGroove(100.0, 16.0, [](int) { return 0.0; });
        const auto result = analyzeTempo(audio.data(), audio.size(), rate);
        const auto text = describeTempo(result, "the mix", 0.0, 16.0, 100.0);
        check(text.find("Estimated tempo") != std::string::npos && text.find("matches it") != std::string::npos,
              "the report names the tempo and compares it with the project's");
    }

    std::cout << (failures == 0 ? "ALL PASSED" : "FAILURES: " + std::to_string(failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
