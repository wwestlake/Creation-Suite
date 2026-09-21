#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Measuring the pitch of a recording precisely enough to say which notes are sharp or flat.
//
// One voice or instrument at a time (a single line of notes), which is what a vocal or lead track is: it tracks the
// fundamental frequency with the YIN method, then groups steady stretches into notes and reports, for each, the
// frequency, the nearest note and how many cents sharp or flat it is (a cent is a hundredth of a semitone). Chords
// and full mixes are not measured reliably by this; frames it is unsure of are left out, not guessed. Pure C++, no
// dependencies, so it can also be used from a tool, a test, or an offline job.
namespace creation::audio
{
struct PitchOptions
{
    double referenceA = 440.0;        // tuning of A4, in Hz
    double minHz = 70.0;              // lowest pitch looked for
    double maxHz = 1200.0;            // highest pitch looked for
    double hopSeconds = 0.02;         // time between measurements
    double confidenceThreshold = 0.85;   // 0..1; frames below this are treated as unpitched
    double silenceDb = -55.0;         // frames quieter than this are silence
    double minNoteSeconds = 0.10;     // shorter stretches are not reported as notes
    double semitoneTolerance = 0.65;  // how far the pitch may wander (in semitones) before a new note starts
};

struct PitchFrame
{
    double timeSeconds = 0.0;
    double frequencyHz = 0.0;         // 0 when the frame is silent or unpitched
    double confidence = 0.0;          // 0..1
    double levelDb = -120.0;          // loudness of the frame (RMS, dBFS)
};

struct Note
{
    double startSeconds = 0.0;
    double endSeconds = 0.0;
    int midiNote = 0;                 // 69 = A4
    std::string name;                 // "A4", "C#3"
    double frequencyHz = 0.0;         // the note's typical (median) frequency
    double cents = 0.0;               // how far from the note's exact pitch: positive = sharp, negative = flat
    double centsSpread = 0.0;         // how much the pitch wandered inside the note (vibrato, wobble)
    double confidence = 0.0;
    double levelDb = -120.0;
    // How the pitch moves across the note: the end compared with the start, in cents (positive = rises). A large value
    // is a bend, a scoop or a slide, which is technique and not necessarily out of tune.
    double motionCents = 0.0;
    // Vibrato: how fast (cycles a second) and how deep (cents either side) the pitch oscillates; 0 when there is none.
    double vibratoRateHz = 0.0;
    double vibratoDepthCents = 0.0;
};

// The frequency of each moment. `samples` is mono audio.
std::vector<PitchFrame> trackPitch(const float* samples, size_t count, double sampleRate, const PitchOptions& options = {});

// Groups pitch frames into notes.
std::vector<Note> findNotes(const std::vector<PitchFrame>& frames, const PitchOptions& options = {});

// Convenience: audio in, notes out.
std::vector<Note> analyzeNotes(const float* samples, size_t count, double sampleRate, const PitchOptions& options = {});

// The notes as a short report a person or an assistant can read: how many, how far off on average, how many are
// noticeably sharp or flat, then each note with its time, name, cents and wobble. `label` names what was measured
// (for example "track 3 \"Vocal\""). At most `maxRows` notes are listed; the report says how many were left out.
std::string describeNotes(const std::vector<Note>& notes, const std::string& label, double startSeconds, double endSeconds,
                          double referenceA = 440.0, size_t maxRows = 60);

std::string noteName(int midiNote);
double midiFromHz(double hz, double referenceA = 440.0);
double hzFromMidi(double midi, double referenceA = 440.0);
}
