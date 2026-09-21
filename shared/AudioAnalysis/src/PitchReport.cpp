#include "creation/audio/PitchAnalysis.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace creation::audio
{
namespace
{
std::string clockText(double seconds)
{
    const int minutes = (int) (seconds / 60.0);
    char text[32];
    std::snprintf(text, sizeof(text), "%d:%05.2f", minutes, seconds - 60.0 * minutes);
    return text;
}

std::string signedCents(double cents)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%+.0f", cents);
    return text;
}
}

std::string describeNotes(const std::vector<Note>& notes, const std::string& label, double startSeconds, double endSeconds,
                          double referenceA, size_t maxRows)
{
    char line[256];
    std::string report = "Pitch analysis of " + label + ", " + clockText(startSeconds) + " to " + clockText(endSeconds)
                         + " (A4 = " + std::to_string((int) std::lround(referenceA)) + " Hz).\n";
    if (notes.empty())
        return report + "No steady notes were found. The track may be silent, may be noise or percussion, or may be several "
                        "notes at once (this measures a single melodic line, so chords and full mixes are skipped).\n";

    double sumAbs = 0.0, sumSigned = 0.0;
    int sharp = 0, flat = 0;
    for (const auto& n : notes)
    {
        sumAbs += std::abs(n.cents);
        sumSigned += n.cents;
        if (n.cents > 20.0) ++sharp;
        else if (n.cents < -20.0) ++flat;
    }
    const double count = (double) notes.size();
    const double mean = sumSigned / count;
    std::snprintf(line, sizeof(line),
                  "Found %zu notes. On average they are %.0f cents from exact pitch (%+.0f cents overall: %s). "
                  "%d are more than 20 cents sharp and %d more than 20 cents flat.\n",
                  notes.size(), sumAbs / count, mean, mean > 5.0 ? "leaning sharp" : mean < -5.0 ? "leaning flat" : "balanced",
                  sharp, flat);
    report += line;
    report += "Notes: time range, note, cents from exact pitch (+ sharp, - flat), wobble in cents, level.\n";

    const size_t shown = std::min(maxRows, notes.size());
    for (size_t i = 0; i < shown; ++i)
    {
        const auto& n = notes[i];
        std::snprintf(line, sizeof(line), "  %s-%s  %-3s  %s cents %s  +/-%.0f  %.0f dB\n", clockText(n.startSeconds).c_str(),
                      clockText(n.endSeconds).c_str(), n.name.c_str(), signedCents(n.cents).c_str(),
                      n.cents > 20.0 ? "(sharp)" : n.cents < -20.0 ? "(flat)" : "(close)", n.centsSpread, n.levelDb);
        // What the note did besides sit on its pitch.
        std::string extra;
        if (std::abs(n.motionCents) >= 15.0)
        {
            std::snprintf(line, sizeof(line), "  %s %.0f cents across the note", n.motionCents > 0 ? "rises" : "falls", std::abs(n.motionCents));
            extra += line;
        }
        if (n.vibratoRateHz > 0.0)
        {
            std::snprintf(line, sizeof(line), "  vibrato %.1f Hz +/-%.0f cents", n.vibratoRateHz, n.vibratoDepthCents);
            extra += line;
        }
        if (! extra.empty())
        {
            report.pop_back();   // the row's newline: the extras belong on the same row
            report += extra + "\n";
        }
    }
    if (notes.size() > shown)
        report += "  ... " + std::to_string(notes.size() - shown) + " more notes not listed.\n";
    report += "This is measured from the recording of a single melodic line; frames it could not measure reliably are left out.\n";
    return report;
}
}
