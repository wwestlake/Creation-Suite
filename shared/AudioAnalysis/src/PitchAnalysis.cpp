#include "creation/audio/PitchAnalysis.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace creation::audio
{
namespace
{
constexpr double pi = 3.14159265358979323846;

double toDb(double amplitude)
{
    return amplitude <= 1.0e-6 ? -120.0 : 20.0 * std::log10(amplitude);
}

double medianOf(std::vector<double> values)
{
    if (values.empty())
        return 0.0;
    std::sort(values.begin(), values.end());
    const size_t middle = values.size() / 2;
    return values.size() % 2 ? values[middle] : 0.5 * (values[middle - 1] + values[middle]);
}
}

double midiFromHz(double hz, double referenceA)
{
    return 69.0 + 12.0 * std::log2(hz / referenceA);
}

double hzFromMidi(double midi, double referenceA)
{
    return referenceA * std::pow(2.0, (midi - 69.0) / 12.0);
}

std::string noteName(int midiNote)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int octave = midiNote / 12 - 1;
    return std::string(names[((midiNote % 12) + 12) % 12]) + std::to_string(octave);
}

std::vector<PitchFrame> trackPitch(const float* samples, size_t count, double sampleRate, const PitchOptions& options)
{
    std::vector<PitchFrame> frames;
    if (samples == nullptr || count == 0 || sampleRate <= 0.0)
        return frames;

    // Work at about 24 kHz or lower: plenty for pitch, and much faster. A plain average of neighbouring samples is
    // enough of a low-pass for that.
    const int decimate = std::max(1, (int) std::floor(sampleRate / 24000.0));
    const double rate = sampleRate / decimate;
    std::vector<float> audio;
    audio.reserve(count / (size_t) decimate + 1);
    for (size_t i = 0; i + (size_t) decimate <= count; i += (size_t) decimate)
    {
        double sum = 0.0;
        for (int k = 0; k < decimate; ++k)
            sum += samples[i + (size_t) k];
        audio.push_back((float) (sum / decimate));
    }

    const int minLag = std::max(2, (int) std::floor(rate / options.maxHz));
    const int maxLag = std::max(minLag + 2, (int) std::ceil(rate / options.minHz));
    const int window = maxLag * 2;
    const int hop = std::max(1, (int) std::round(options.hopSeconds * rate));

    std::vector<double> difference((size_t) maxLag + 1);
    std::vector<double> normalised((size_t) maxLag + 1);

    for (long start = 0; start + window + maxLag <= (long) audio.size(); start += hop)
    {
        PitchFrame frame;
        frame.timeSeconds = (double) (start + window / 2) / rate;

        // Loudness of the frame.
        double energy = 0.0;
        for (int i = 0; i < window; ++i)
            energy += (double) audio[(size_t) (start + i)] * audio[(size_t) (start + i)];
        frame.levelDb = toDb(std::sqrt(energy / window));
        if (frame.levelDb < options.silenceDb)
        {
            frames.push_back(frame);
            continue;
        }

        // YIN step 1: the difference function d(tau).
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            double sum = 0.0;
            for (int i = 0; i < window; ++i)
            {
                const double delta = (double) audio[(size_t) (start + i)] - (double) audio[(size_t) (start + i + tau)];
                sum += delta * delta;
            }
            difference[(size_t) tau] = sum;
        }

        // Step 2: cumulative mean normalised difference.
        normalised[0] = 1.0;
        double running = 0.0;
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            running += difference[(size_t) tau];
            normalised[(size_t) tau] = running > 0.0 ? difference[(size_t) tau] * tau / running : 1.0;
        }

        // Step 3: the first dip below the threshold, followed down to its bottom.
        const double threshold = 1.0 - options.confidenceThreshold;
        int best = -1;
        for (int tau = minLag; tau < maxLag; ++tau)
        {
            if (normalised[(size_t) tau] < threshold)
            {
                while (tau + 1 < maxLag && normalised[(size_t) tau + 1] < normalised[(size_t) tau])
                    ++tau;
                best = tau;
                break;
            }
        }

        if (best > 0)
        {
            // Step 4: parabolic interpolation around the dip for a lag finer than one sample.
            double lag = (double) best;
            if (best > 1 && best < maxLag)
            {
                const double a = normalised[(size_t) best - 1];
                const double b = normalised[(size_t) best];
                const double c = normalised[(size_t) best + 1];
                const double denominator = a - 2.0 * b + c;
                if (std::abs(denominator) > 1.0e-12)
                    lag += 0.5 * (a - c) / denominator;
            }
            frame.frequencyHz = rate / lag;
            frame.confidence = std::clamp(1.0 - normalised[(size_t) best], 0.0, 1.0);
        }
        frames.push_back(frame);
    }
    return frames;
}

std::vector<Note> findNotes(const std::vector<PitchFrame>& frames, const PitchOptions& options)
{
    std::vector<Note> notes;
    size_t i = 0;
    while (i < frames.size())
    {
        if (frames[i].frequencyHz <= 0.0)
        {
            ++i;
            continue;
        }

        // Grow a note while the pitch stays within the tolerance of where it started.
        const double anchor = midiFromHz(frames[i].frequencyHz, options.referenceA);
        size_t end = i;
        int unpitchedRun = 0;
        std::vector<double> midi;
        std::vector<double> levels;
        double confidenceSum = 0.0;
        size_t lastVoiced = i;
        for (size_t j = i; j < frames.size(); ++j)
        {
            if (frames[j].frequencyHz <= 0.0)
            {
                if (++unpitchedRun > 2)
                    break;   // a longer gap ends the note
                continue;
            }
            unpitchedRun = 0;
            const double m = midiFromHz(frames[j].frequencyHz, options.referenceA);
            if (std::abs(m - anchor) > options.semitoneTolerance)
                break;
            midi.push_back(m);
            levels.push_back(frames[j].levelDb);
            confidenceSum += frames[j].confidence;
            lastVoiced = j;
            end = j;
        }

        const double hopSeconds = frames.size() > 1 ? frames[1].timeSeconds - frames[0].timeSeconds : options.hopSeconds;
        const double startSeconds = frames[i].timeSeconds - 0.5 * hopSeconds;
        const double endSeconds = frames[lastVoiced].timeSeconds + 0.5 * hopSeconds;
        if (midi.size() >= 2 && endSeconds - startSeconds >= options.minNoteSeconds)
        {
            Note note;
            note.startSeconds = std::max(0.0, startSeconds);
            note.endSeconds = endSeconds;
            // Judge the tuning from the steady middle of the note: the first and last moments are where the pitch is still
            // landing on the note or leaving it (a slide between notes), not where it sits.
            const size_t trim = midi.size() >= 10 ? midi.size() / 5 : (midi.size() >= 6 ? 1 : 0);
            const std::vector<double> steady(midi.begin() + (long) trim, midi.end() - (long) trim);
            const double typicalMidi = medianOf(steady);
            note.midiNote = (int) std::lround(typicalMidi);
            note.name = noteName(note.midiNote);
            note.frequencyHz = hzFromMidi(typicalMidi, options.referenceA);
            note.cents = 100.0 * (typicalMidi - note.midiNote);

            double sumSquares = 0.0;
            for (double m : steady)
                sumSquares += (m - typicalMidi) * (m - typicalMidi);
            note.centsSpread = 100.0 * std::sqrt(sumSquares / (double) steady.size());
            note.confidence = confidenceSum / (double) midi.size();
            note.levelDb = medianOf(levels);

            // Motion across the note: the last quarter compared with the first quarter.
            if (midi.size() >= 6)
            {
                const size_t quarter = std::max<size_t>(1, midi.size() / 4);
                const double first = medianOf(std::vector<double>(midi.begin(), midi.begin() + (long) quarter));
                const double last = medianOf(std::vector<double>(midi.end() - (long) quarter, midi.end()));
                note.motionCents = 100.0 * (last - first);
            }

            // Vibrato: take out the overall slope, then count how often what is left crosses zero. Slow, wide and regular
            // movement of a few cycles a second is vibrato; small or irregular movement is not reported as one.
            if (midi.size() >= 20)
            {
                const double n = (double) midi.size();
                double sumX = 0.0, sumY = 0.0, sumXY = 0.0, sumXX = 0.0;
                for (size_t k = 0; k < midi.size(); ++k)
                {
                    sumX += (double) k;
                    sumY += midi[k];
                    sumXY += (double) k * midi[k];
                    sumXX += (double) k * (double) k;
                }
                const double slope = (n * sumXY - sumX * sumY) / (n * sumXX - sumX * sumX);
                const double intercept = (sumY - slope * sumX) / n;

                std::vector<double> residual(midi.size());
                double sumSq = 0.0;
                for (size_t k = 0; k < midi.size(); ++k)
                {
                    residual[k] = 100.0 * (midi[k] - (intercept + slope * (double) k));   // cents
                    sumSq += residual[k] * residual[k];
                }
                int crossings = 0;
                int side = 0;
                for (double r : residual)
                {
                    const int now = r > 3.0 ? 1 : r < -3.0 ? -1 : 0;   // a few cents of margin so noise does not count
                    if (now != 0 && side != 0 && now != side)
                        ++crossings;
                    if (now != 0)
                        side = now;
                }
                const double seconds = n * hopSeconds;
                const double rate = crossings / 2.0 / seconds;
                const double depth = 1.4142 * std::sqrt(sumSq / n);
                if (seconds >= 0.4 && crossings >= 4 && rate >= 3.5 && rate <= 9.0 && depth >= 10.0)
                {
                    note.vibratoRateHz = rate;
                    note.vibratoDepthCents = depth;
                }
            }
            notes.push_back(note);
        }
        i = std::max(end, i) + 1;
    }
    return notes;
}

std::vector<Note> analyzeNotes(const float* samples, size_t count, double sampleRate, const PitchOptions& options)
{
    return findNotes(trackPitch(samples, count, sampleRate, options), options);
}
}
