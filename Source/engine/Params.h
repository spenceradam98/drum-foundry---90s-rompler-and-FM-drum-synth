// Parameter structs shared by the engine, presets and plugin wrapper.
#pragma once

#include <array>
#include <string>

namespace df
{
constexpr int kNumPads = 8;
constexpr int kNumOps = 4;
constexpr int kNumAlgorithms = 8;
constexpr int kMaxVoices = 16;
constexpr int kNumOutBuses = 9;   // 0 = main, 1..8 = multi-outs

enum class FilterType { LowPass = 0, BandPass, HighPass };

struct OpParams
{
    float ratio = 1.0f;     // 0.25 .. 16, multiple of the voice pitch
    float level = 0.0f;     // 0..1: amplitude for carriers, modulation index for modulators
    float decayMs = 200.0f; // time to -60 dB
};

struct VoiceParams
{
    // FM
    int algorithm = 0;                 // 0..7
    std::array<OpParams, kNumOps> op {};
    float feedback = 0.0f;             // op 4 self-feedback 0..1
    float pitchHz = 100.0f;            // base pitch, 20..5000 Hz
    float pitchEnvSemis = 0.0f;        // 0..48 semitones of downward sweep
    float pitchEnvMs = 30.0f;          // sweep time
    float fmLevel = 1.0f;              // 0..1

    // Transient (sample) layer
    int sample = 0;                    // index into the built-in set
    float sampleStart = 0.0f;          // 0..1 of the sample length
    float sampleLevel = 0.0f;          // 0..1
    float samplePitch = 0.0f;          // semitones -24..24
    float sampleBits = 12.0f;          // 4..16
    float sampleRateKHz = 26.0f;       // 8..44.1

    // Noise
    float noiseLevel = 0.0f;           // 0..1
    float noiseColour = 0.0f;          // -1 dark .. 0 white .. +1 bright
    float noiseDecayMs = 120.0f;

    // Shape
    FilterType filter = FilterType::LowPass;
    float cutoffHz = 18000.0f;
    float resonance = 0.0f;            // 0..1
    float attackMs = 0.0f;
    float holdMs = 5.0f;
    float decayMs = 400.0f;
    float drive = 0.0f;                // 0..1

    // Out
    float levelDb = 0.0f;              // -60..+6
    float pan = 0.0f;                  // -1..1
    int chokeGroup = 0;                // 0 = none, 1..4
    int outBus = 0;                    // 0 = main, 1..8
    int note = 36;                     // MIDI note that triggers this pad
};

struct GlobalParams
{
    float masterCrush = 0.0f;   // 0..1
    float outputDb = 0.0f;      // -24..+12
};

struct Kit
{
    std::string name;
    std::array<VoiceParams, kNumPads> pads {};
};

// GM default notes per pad: Kick, Snare, Clap, Closed Hat, Open Hat, Tom, Cymbal, Perc.
constexpr std::array<int, kNumPads> kDefaultNotes { 36, 38, 39, 42, 46, 45, 49, 37 };
const char* padName (int pad);
} // namespace df
