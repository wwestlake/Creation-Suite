#include "creation/audio/TempoAnalysis.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <numeric>

namespace creation::audio
{
namespace
{
constexpr double pi = 3.14159265358979323846;

// A plain radix-2 FFT; `data.size()` must be a power of two.
void fft(std::vector<std::complex<double>>& data)
{
    const size_t n = data.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(data[i], data[j]);
    }
    for (size_t length = 2; length <= n; length <<= 1)
    {
        const double angle = -2.0 * pi / (double) length;
        const std::complex<double> step(std::cos(angle), std::sin(angle));
        for (size_t start = 0; start < n; start += length)
        {
            std::complex<double> w(1.0, 0.0);
            for (size_t k = 0; k < length / 2; ++k)
            {
                const auto a = data[start + k];
                const auto b = data[start + k + length / 2] * w;
                data[start + k] = a + b;
                data[start + k + length / 2] = a - b;
                w *= step;
            }
        }
    }
}

// How strongly the sound changes at each moment (spectral flux over log-compressed magnitudes), at `fps` values a second,
// with the slow-moving average removed and normalised so its standard deviation is 1.
std::vector<double> onsetEnvelope(const float* samples, size_t count, double sampleRate, double hopSeconds, double& fpsOut)
{
    constexpr size_t window = 1024;
    const size_t hop = std::max<size_t>(1, (size_t) std::llround(hopSeconds * sampleRate));
    fpsOut = sampleRate / (double) hop;

    std::vector<double> hann(window);
    for (size_t i = 0; i < window; ++i)
        hann[i] = 0.5 - 0.5 * std::cos(2.0 * pi * (double) i / (double) (window - 1));

    // Use bins up to about 8 kHz: attacks show there, and cymbal hiss beyond it adds little.
    const size_t topBin = std::min<size_t>(window / 2, (size_t) (8000.0 / sampleRate * (double) window) + 1);

    std::vector<double> flux;
    std::vector<double> previous(topBin, 0.0);
    std::vector<std::complex<double>> buffer(window);
    for (size_t start = 0; start + window <= count; start += hop)
    {
        for (size_t i = 0; i < window; ++i)
            buffer[i] = std::complex<double>((double) samples[start + i] * hann[i], 0.0);
        fft(buffer);

        double sum = 0.0;
        for (size_t k = 1; k < topBin; ++k)
        {
            const double magnitude = std::log(1.0 + 100.0 * std::abs(buffer[k]));
            sum += std::max(0.0, magnitude - previous[k]);
            previous[k] = magnitude;
        }
        flux.push_back(sum);
    }
    if (flux.size() < 8)
        return flux;

    // Remove the local average (about half a second either side) and rectify, then scale.
    const size_t half = std::max<size_t>(2, (size_t) std::llround(0.5 * fpsOut));
    std::vector<double> envelope(flux.size());
    for (size_t i = 0; i < flux.size(); ++i)
    {
        const size_t from = i > half ? i - half : 0;
        const size_t to = std::min(flux.size(), i + half + 1);
        double mean = 0.0;
        for (size_t j = from; j < to; ++j)
            mean += flux[j];
        mean /= (double) (to - from);
        envelope[i] = std::max(0.0, flux[i] - mean);
    }
    double mean = std::accumulate(envelope.begin(), envelope.end(), 0.0) / (double) envelope.size();
    double variance = 0.0;
    for (double v : envelope)
        variance += (v - mean) * (v - mean);
    const double deviation = std::sqrt(variance / (double) envelope.size());
    if (deviation > 1.0e-9)
        for (double& v : envelope)
            v /= deviation;
    return envelope;
}

// The lag (in frames) at which the envelope best repeats, with a mild preference for everyday tempos.
double bestPeriod(const std::vector<double>& envelope, double fps, double minBpm, double maxBpm, double expectedBpm, double& confidence)
{
    const size_t minLag = std::max<size_t>(2, (size_t) std::floor(fps * 60.0 / maxBpm));
    const size_t maxLag = std::min(envelope.size() / 2, (size_t) std::ceil(fps * 60.0 / minBpm));
    if (maxLag <= minLag + 2)
        return 0.0;

    std::vector<double> autocorrelation(maxLag * 4 + 1, 0.0);
    for (size_t lag = 1; lag < autocorrelation.size() && lag < envelope.size() / 2 + 1; ++lag)
    {
        double sum = 0.0;
        for (size_t i = 0; i + lag < envelope.size(); ++i)
            sum += envelope[i] * envelope[i + lag];
        autocorrelation[lag] = sum / (double) (envelope.size() - lag);
    }

    // Loose playing puts each attack a few frames off, which smears the repeat of the pattern over a few lags, so
    // lags are compared by the repeat within a small window around them (about 30 ms either way at 10 ms frames).
    const size_t tolerance = std::max<size_t>(1, (size_t) std::lround(0.030 * fps));
    auto nearby = [&](size_t lag)
    {
        double strongest = 0.0;
        for (size_t l = lag > tolerance ? lag - tolerance : 1; l <= lag + tolerance && l < autocorrelation.size(); ++l)
            strongest = std::max(strongest, autocorrelation[l]);
        return strongest;
    };

    auto scoreAt = [&](size_t lag)
    {
        // The beat period, plus its multiples (a steady beat also repeats every two, four beats).
        double score = nearby(lag);
        if (lag * 2 < autocorrelation.size()) score += 0.5 * nearby(lag * 2);
        if (lag * 4 < autocorrelation.size()) score += 0.25 * nearby(lag * 4);
        const double bpm = fps * 60.0 / (double) lag;
        const double centre = expectedBpm > 0.0 ? expectedBpm : 110.0;
        const double octaves = std::log2(bpm / centre);
        return score * std::exp(-0.5 * (octaves / 1.0) * (octaves / 1.0));
    };

    size_t best = minLag;
    double bestScore = -1.0;
    double total = 0.0;
    for (size_t lag = minLag; lag <= maxLag; ++lag)
    {
        const double score = scoreAt(lag);
        total += std::max(0.0, score);
        if (score > bestScore)
        {
            bestScore = score;
            best = lag;
        }
    }
    const double average = total / (double) (maxLag - minLag + 1);
    confidence = average > 1.0e-9 ? std::clamp((bestScore / average - 1.0) / 6.0, 0.0, 1.0) : 0.0;

    // The window chose the region. Inside it the period is the centre of the repeat, not its single tallest point, which
    // loose playing moves around: take the centre of mass of everything above half the peak.
    {
        const size_t from = best > tolerance ? best - tolerance : 1;
        const size_t to = std::min(best + tolerance, autocorrelation.size() - 1);
        double peak = 0.0;
        for (size_t l = from; l <= to; ++l)
            peak = std::max(peak, autocorrelation[l]);
        double weightSum = 0.0, position = 0.0;
        for (size_t l = from; l <= to; ++l)
        {
            const double w = autocorrelation[l] - 0.5 * peak;
            if (w > 0.0)
            {
                weightSum += w;
                position += w * (double) l;
            }
        }
        if (weightSum > 0.0)
            return position / weightSum;
    }

    // Refine to a fraction of a frame.
    double lag = (double) best;
    if (best > minLag && best < maxLag)
    {
        const double a = autocorrelation[best - 1], b = autocorrelation[best], c = autocorrelation[best + 1];
        const double denominator = a - 2.0 * b + c;
        if (std::abs(denominator) > 1.0e-12)
            lag += std::clamp(0.5 * (a - c) / denominator, -0.5, 0.5);
    }
    return lag;
}

// Dynamic programming beat tracker: choose the beat frames whose attacks are strong and whose spacing stays close to the
// period.
std::vector<size_t> trackBeats(const std::vector<double>& envelope, double period)
{
    const size_t n = envelope.size();
    std::vector<double> score(n, 0.0);
    std::vector<int> back(n, -1);
    const size_t low = std::max<size_t>(1, (size_t) std::floor(period / 2.0));
    const size_t high = (size_t) std::ceil(period * 2.0);
    const double tightness = 100.0;

    for (size_t t = 0; t < n; ++t)
    {
        double bestPrevious = 0.0;
        int bestIndex = -1;
        for (size_t gap = low; gap <= high && gap <= t; ++gap)
        {
            const size_t p = t - gap;
            const double ratio = std::log((double) gap / period);
            const double candidate = score[p] - tightness * ratio * ratio;
            if (bestIndex < 0 || candidate > bestPrevious)
            {
                bestPrevious = candidate;
                bestIndex = (int) p;
            }
        }
        score[t] = envelope[t] + (bestIndex >= 0 ? bestPrevious : 0.0);
        back[t] = bestIndex;
    }

    // Start from the best score in the last stretch of the recording, and follow the chain back.
    const size_t tailStart = n > (size_t) period ? n - (size_t) period : 0;
    size_t end = tailStart;
    for (size_t t = tailStart; t < n; ++t)
        if (score[t] > score[end])
            end = t;

    std::vector<size_t> beats;
    for (int t = (int) end; t >= 0; t = back[(size_t) t])
    {
        beats.push_back((size_t) t);
        if (back[(size_t) t] < 0)
            break;
    }
    std::reverse(beats.begin(), beats.end());
    return beats;
}
}

TempoResult analyzeTempo(const float* samples, size_t count, double sampleRate, const TempoOptions& options)
{
    TempoResult result;
    if (samples == nullptr || sampleRate <= 0.0 || count < (size_t) (sampleRate * 4.0))
        return result;   // too short to judge a tempo

    double fps = 0.0;
    const auto envelope = onsetEnvelope(samples, count, sampleRate, options.hopSeconds, fps);
    if (envelope.size() < (size_t) (fps * 3.0))
        return result;

    double confidence = 0.0;
    const double period = bestPeriod(envelope, fps, options.minBpm, options.maxBpm, options.expectedBpm, confidence);
    if (period <= 0.0)
        return result;

    result.found = true;
    result.confidence = confidence;
    result.bpm = fps * 60.0 / period;
    result.beatSeconds = 60.0 / result.bpm;

    // The attacks: local peaks well above the noise, at least 50 ms apart.
    const size_t minGap = std::max<size_t>(1, (size_t) std::llround(0.05 * fps));
    for (size_t i = 1; i + 1 < envelope.size(); ++i)
    {
        if (envelope[i] > 1.0 && envelope[i] >= envelope[i - 1] && envelope[i] > envelope[i + 1])
        {
            if (! result.onsets.empty() && (double) i / fps - result.onsets.back().timeSeconds < (double) minGap / fps)
            {
                if (envelope[i] > result.onsets.back().strength)
                    result.onsets.back() = { (double) i / fps, envelope[i] };
                continue;
            }
            result.onsets.push_back({ (double) i / fps, envelope[i] });
        }
    }

    const auto beatFrames = trackBeats(envelope, period);
    for (size_t frame : beatFrames)
        result.beats.push_back((double) frame / fps);

    // Steadiness.
    if (result.beats.size() >= 4)
    {
        std::vector<double> gaps;
        for (size_t i = 1; i < result.beats.size(); ++i)
            gaps.push_back(result.beats[i] - result.beats[i - 1]);
        const double mean = std::accumulate(gaps.begin(), gaps.end(), 0.0) / (double) gaps.size();
        double variance = 0.0;
        for (double g : gaps)
            variance += (g - mean) * (g - mean);
        result.beatJitterMs = 1000.0 * std::sqrt(variance / (double) gaps.size());

        const size_t half = gaps.size() / 2;
        if (half >= 2)
        {
            const double first = std::accumulate(gaps.begin(), gaps.begin() + (long) half, 0.0) / (double) half;
            const double last = std::accumulate(gaps.begin() + (long) half, gaps.end(), 0.0) / (double) (gaps.size() - half);
            result.driftBpm = 60.0 / last - 60.0 / first;
        }
    }

    // Tightness: each attack against the grid the player is actually using. The grid is whichever of eighths (2 to a
    // beat), triplets or swing (3), sixteenths (4) and sextuplets (6) the attacks sit closest to, judged against that
    // grid's own spacing, so a swung or triplet groove is not called loose for not being straight.
    if (result.beats.size() >= 2 && ! result.onsets.empty())
    {
        auto gridFor = [&](int perBeat)
        {
            std::vector<double> grid;
            for (size_t i = 0; i + 1 < result.beats.size(); ++i)
                for (int s = 0; s < perBeat; ++s)
                    grid.push_back(result.beats[i] + (result.beats[i + 1] - result.beats[i]) * (double) s / (double) perBeat);
            grid.push_back(result.beats.back());
            return grid;
        };

        auto offsetsFor = [&](int perBeat)
        {
            const auto grid = gridFor(perBeat);
            std::vector<double> offsets;
            for (const auto& onset : result.onsets)
            {
                if (onset.timeSeconds < result.beats.front() || onset.timeSeconds > result.beats.back())
                    continue;
                const auto next = std::lower_bound(grid.begin(), grid.end(), onset.timeSeconds);
                double nearest = *next;
                if (next != grid.begin() && (next == grid.end() || onset.timeSeconds - *(next - 1) < nearest - onset.timeSeconds))
                    nearest = *(next - 1);
                offsets.push_back(1000.0 * (onset.timeSeconds - nearest));
            }
            return offsets;
        };

        int chosen = 4;
        std::vector<double> chosenOffsets;
        if (options.subdivisionsPerBeat >= 1.0)
        {
            chosen = std::max(1, (int) std::lround(options.subdivisionsPerBeat));
            chosenOffsets = offsetsFor(chosen);
        }
        else
        {
            double bestScore = 1.0e9;
            for (int perBeat : { 2, 3, 4, 6 })
            {
                const auto offsets = offsetsFor(perBeat);
                if (offsets.empty())
                    continue;
                double abs = 0.0;
                for (double o : offsets)
                    abs += std::abs(o);
                const double spacingMs = 1000.0 * result.beatSeconds / (double) perBeat;
                // Error relative to the grid's spacing, with a small preference for the simpler grid (finer grids always fit
                // a little better by chance).
                const double score = (abs / (double) offsets.size()) / spacingMs + 0.03 * perBeat;
                if (score < bestScore)
                {
                    bestScore = score;
                    chosen = perBeat;
                    chosenOffsets = offsets;
                }
            }
        }

        result.gridSubdivisions = chosen;
        result.feel = chosen == 2 ? "straight eighth notes" : chosen == 3 ? "triplets or swing" : chosen == 4 ? "straight sixteenth notes"
                    : chosen == 6 ? "sextuplets" : std::to_string(chosen) + " steps to a beat";

        if (! chosenOffsets.empty())
        {
            result.onsetsMeasured = chosenOffsets.size();
            double abs = 0.0, signedSum = 0.0;
            for (double o : chosenOffsets)
            {
                abs += std::abs(o);
                signedSum += o;
            }
            result.meanAbsOffsetMs = abs / (double) chosenOffsets.size();
            result.leanMs = signedSum / (double) chosenOffsets.size();
            double variance = 0.0;
            for (double o : chosenOffsets)
                variance += (o - result.leanMs) * (o - result.leanMs);
            result.spreadMs = std::sqrt(variance / (double) chosenOffsets.size());
        }

        // The pattern, bar by bar (four beats to a bar, counted from the first beat found): one mark for each step of the
        // grid. X is a strong attack, x a lighter one, . nothing.
        const size_t beatsPerBar = 4;
        for (size_t bar = 0; (bar + 1) * beatsPerBar < result.beats.size() && bar < 8; ++bar)
        {
            std::string row;
            for (size_t beat = 0; beat < beatsPerBar; ++beat)
            {
                const size_t b = bar * beatsPerBar + beat;
                if (beat > 0)
                    row += "| ";
                for (int step = 0; step < chosen; ++step)
                {
                    const double time = result.beats[b] + (result.beats[b + 1] - result.beats[b]) * (double) step / (double) chosen;
                    const double half = 0.5 * (result.beats[b + 1] - result.beats[b]) / (double) chosen;
                    char mark = '.';
                    for (const auto& onset : result.onsets)
                    {
                        if (std::abs(onset.timeSeconds - time) <= half)
                        {
                            const char here = onset.strength >= 3.0 ? 'X' : 'x';
                            if (mark == '.' || here == 'X')
                                mark = here;
                        }
                    }
                    row += mark;
                    row += ' ';
                }
            }
            result.pattern.push_back(row);
        }
    }
    return result;
}

namespace
{
std::string clockText(double seconds)
{
    const int minutes = (int) (seconds / 60.0);
    char text[32];
    std::snprintf(text, sizeof(text), "%d:%05.2f", minutes, seconds - 60.0 * minutes);
    return text;
}
}

std::string describeTempo(const TempoResult& result, const std::string& label, double startSeconds, double endSeconds, double projectBpm)
{
    char line[320];
    std::string report = "Tempo analysis of " + label + ", " + clockText(startSeconds) + " to " + clockText(endSeconds) + ".\n";
    if (! result.found)
        return report + "Could not measure a tempo: the section is too short (it needs at least a few seconds) or has almost no "
                        "attacks to measure, as with a sustained pad or a smooth vocal line.\n";

    const char* sure = result.confidence > 0.6 ? "high" : result.confidence > 0.3 ? "medium" : "low";
    std::snprintf(line, sizeof(line), "Estimated tempo: %.1f BPM (a beat every %.3f s), confidence %s (%.2f). Found %zu attacks and %zu beats.\n",
                  result.bpm, result.beatSeconds, sure, result.confidence, result.onsets.size(), result.beats.size());
    report += line;

    if (projectBpm > 0.0)
    {
        const double ratio = result.bpm / projectBpm;
        if (std::abs(ratio - 1.0) < 0.03)
            std::snprintf(line, sizeof(line), "The project tempo is %.1f BPM: this matches it.\n", projectBpm);
        else if (std::abs(ratio - 2.0) < 0.06 || std::abs(ratio - 0.5) < 0.03)
            std::snprintf(line, sizeof(line), "The project tempo is %.1f BPM: this is %s time against it, which usually means the same rhythm counted twice as %s.\n",
                          projectBpm, ratio > 1.0 ? "double" : "half", ratio > 1.0 ? "fast" : "slow");
        else
            std::snprintf(line, sizeof(line), "The project tempo is %.1f BPM, which differs from what was measured (%+.1f BPM).\n", projectBpm, result.bpm - projectBpm);
        report += line;
    }

    if (result.beats.size() >= 4)
    {
        std::snprintf(line, sizeof(line), "Steadiness: the beat spacing varies by about %.0f ms (%.1f%% of a beat)%s.\n", result.beatJitterMs,
                      100.0 * result.beatJitterMs / (1000.0 * result.beatSeconds),
                      std::abs(result.driftBpm) >= 1.0
                          ? (result.driftBpm > 0.0 ? ", and the tempo speeds up over the section" : ", and the tempo slows down over the section")
                          : ", with no noticeable speeding up or slowing down");
        report += line;
        if (std::abs(result.driftBpm) >= 1.0)
        {
            std::snprintf(line, sizeof(line), "  (about %+.1f BPM from the first half to the second)\n", result.driftBpm);
            report += line;
        }
    }

    if (result.onsetsMeasured >= 4)
    {
        const char* lean = result.leanMs < -8.0 ? "ahead of the beat (pushing, rushing)" : result.leanMs > 8.0 ? "behind the beat (laid back, dragging)"
                                                                                                            : "right on the beat overall";
        std::snprintf(line, sizeof(line), "Tightness: %zu attacks measured against the beat grid, on average %.0f ms from the nearest grid line (spread %.0f ms). "
                                          "They sit %s, %+.0f ms on average.\n",
                      result.onsetsMeasured, result.meanAbsOffsetMs, result.spreadMs, lean, result.leanMs);
        report += line;
        report += "For reference, under about 10 ms is very tight, 10 to 25 ms is a natural human feel, and over about 30 ms is loose enough to hear.\n";
        if (! result.feel.empty())
            report += "The hits sit best on a grid of " + result.feel + ", and tightness above is judged against that grid, so a swung or triplet feel is not counted as loose.\n";
    }

    if (! result.pattern.empty())
    {
        report += "Rhythm pattern (four beats to a bar, counted from the first beat found, which may not be the downbeat; one mark per step; "
                  "X strong hit, x lighter hit, . nothing; | separates beats):\n";
        for (size_t i = 0; i < result.pattern.size(); ++i)
            report += "  bar " + std::to_string(i + 1) + ": " + result.pattern[i] + "\n";
    }

    const size_t shown = std::min<size_t>(result.beats.size(), 16);
    if (shown > 0)
    {
        report += "First beats (seconds):";
        for (size_t i = 0; i < shown; ++i)
        {
            std::snprintf(line, sizeof(line), " %.2f", result.beats[i]);
            report += line;
        }
        report += result.beats.size() > shown ? " ...\n" : "\n";
    }
    report += "Measured from the recording's attacks; it works best on drums and other percussive playing and is less certain on sustained sounds.\n";
    return report;
}
}
