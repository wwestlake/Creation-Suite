// Checks the pitch analyzer against sounds whose pitch is known exactly: a sine wave at a chosen note and a chosen number
// of cents sharp or flat, a short melody with a silent gap, and silence.

#include <creation/audio/PitchAnalysis.h>

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
constexpr double rate = 48000.0;

void addTone(std::vector<float>& out, double hz, double seconds, double amplitude = 0.5)
{
    const size_t n = (size_t) (seconds * rate);
    const size_t base = out.size();
    out.resize(base + n);
    for (size_t i = 0; i < n; ++i)
        out[base + i] = (float) (amplitude * std::sin(2.0 * pi * hz * (double) i / rate));
}

void addSilence(std::vector<float>& out, double seconds)
{
    out.resize(out.size() + (size_t) (seconds * rate), 0.0f);
}
}

int main()
{
    using namespace creation::audio;

    // ---- One note, exactly A4 and 18 cents sharp ----
    {
        std::vector<float> audio;
        addTone(audio, hzFromMidi(69.0 + 0.18), 0.8);
        const auto notes = analyzeNotes(audio.data(), audio.size(), rate);
        check(notes.size() == 1, "one steady tone is one note (" + std::to_string(notes.size()) + ")");
        if (! notes.empty())
        {
            check(notes[0].name == "A4", "it is A4: " + notes[0].name);
            check(std::abs(notes[0].cents - 18.0) < 3.0, "it is 18 cents sharp, measured " + std::to_string(notes[0].cents));
            check(std::abs(notes[0].frequencyHz - 444.5) < 1.0, "its frequency is about 444.5 Hz: " + std::to_string(notes[0].frequencyHz));
        }
    }

    // ---- A flat note ----
    {
        std::vector<float> audio;
        addTone(audio, hzFromMidi(60.0 - 0.25), 0.8);   // C4, 25 cents flat
        const auto notes = analyzeNotes(audio.data(), audio.size(), rate);
        check(notes.size() == 1 && notes[0].name == "C4", "a low tone is C4");
        if (! notes.empty())
            check(std::abs(notes[0].cents + 25.0) < 3.0, "it is 25 cents flat, measured " + std::to_string(notes[0].cents));
    }

    // ---- A melody with a gap: E4 (exact), gap, G4 (10 cents sharp), D3 (30 cents flat) ----
    {
        std::vector<float> audio;
        addTone(audio, hzFromMidi(64.0), 0.5);
        addSilence(audio, 0.3);
        addTone(audio, hzFromMidi(67.0 + 0.10), 0.5);
        addTone(audio, hzFromMidi(50.0 - 0.30), 0.6);
        const auto notes = analyzeNotes(audio.data(), audio.size(), rate);
        check(notes.size() == 3, "a three-note melody with a gap gives three notes (" + std::to_string(notes.size()) + ")");
        if (notes.size() == 3)
        {
            check(notes[0].name == "E4" && std::abs(notes[0].cents) < 3.0, "E4, in tune: " + std::to_string(notes[0].cents));
            check(notes[1].name == "G4" && std::abs(notes[1].cents - 10.0) < 3.0, "G4, 10 cents sharp: " + std::to_string(notes[1].cents));
            check(notes[2].name == "D3" && std::abs(notes[2].cents + 30.0) < 3.0, "D3, 30 cents flat: " + std::to_string(notes[2].cents));
            check(notes[0].startSeconds < 0.1 && notes[1].startSeconds > 0.75 && notes[1].startSeconds < 0.9,
                  "the notes start at the right times");
            check(notes[1].endSeconds > 1.2 && notes[1].endSeconds < 1.4, "a note ends where the next begins");
        }
    }

    // ---- Silence and noise give no notes ----
    {
        std::vector<float> audio;
        addSilence(audio, 1.0);
        check(analyzeNotes(audio.data(), audio.size(), rate).empty(), "silence has no notes");

        std::vector<float> noise((size_t) rate);
        unsigned int seed = 12345;
        for (auto& s : noise)
        {
            seed = seed * 1664525u + 1013904223u;
            s = 0.3f * ((float) (seed >> 8) / (float) (1u << 24) * 2.0f - 1.0f);
        }
        check(analyzeNotes(noise.data(), noise.size(), rate).empty(), "noise has no notes");
    }

    // ---- A different reference tuning ----
    {
        std::vector<float> audio;
        addTone(audio, 432.0, 0.8);
        PitchOptions options;
        options.referenceA = 432.0;
        const auto notes = analyzeNotes(audio.data(), audio.size(), rate, options);
        check(notes.size() == 1 && notes[0].name == "A4" && std::abs(notes[0].cents) < 3.0, "432 Hz is in tune when A is 432");
    }

    std::cout << (failures == 0 ? "ALL PASSED" : "FAILURES: " + std::to_string(failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
