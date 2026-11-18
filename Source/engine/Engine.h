// drum-foundry engine: 8 pads, 16-voice polyphony, 4-op FM + crunchy transient + noise per voice,
// 9 stereo output buses (main + 8 multi-outs). No JUCE dependency.
#pragma once

#include <array>
#include <atomic>
#include <vector>

#include "Dsp.h"
#include "Params.h"

namespace df
{
struct Algorithm
{
    const char* name;
    // mod[target][source]: op `source` modulates op `target`. Sources always have a higher index.
    std::array<std::array<bool, kNumOps>, kNumOps> mod;
    std::array<bool, kNumOps> carrier;
};
const Algorithm& algorithm (int index);

// Maps a MIDI note to a pad: exact match on a pad's note first, then GM aliases
// (35 kick, 40 snare, 44 closed hat, 41/43/47/48/50 tom, 51/52/55/57/59 cymbal...). -1 = none.
int padForNote (int note, const std::array<int, kNumPads>& padNotes);

class Engine
{
public:
    Engine();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    void setPad (int pad, const VoiceParams& p) { pads[(size_t) pad] = p; }
    const VoiceParams& getPad (int pad) const { return pads[(size_t) pad]; }
    void setGlobal (const GlobalParams& g) { global = g; }

    // Buses the host has enabled; voices routed to an inactive bus fall back to main.
    void setBusActive (int bus, bool active) { busActive[(size_t) bus] = active || bus == 0; }

    void noteOn (int pad, float velocity /*0..1*/);
    void midiNote (int note, float velocity);   // routes through padForNote
    void allNotesOff();

    // Renders `n` samples (n <= maxBlockSize) into the internal bus buffers.
    void process (int n);
    const float* bus (int b, int ch) const { return buffers[(size_t) (b * 2 + ch)].data(); }

    int activeVoices() const;
    // For the UI: per-pad trigger counter and current loudness (0..1).
    uint32_t padTriggerCount (int pad) const { return triggerCount[(size_t) pad].load(); }
    float padLevel (int pad) const { return padMeter[(size_t) pad].load(); }

    double getSampleRate() const { return fs; }

private:
    struct Voice
    {
        bool active = false;
        int pad = 0;
        uint64_t age = 0;
        float velocity = 1.0f;
        VoiceParams p;          // snapshot at trigger time
        // FM
        std::array<double, kNumOps> phase {};
        std::array<float, kNumOps> opEnv {}, opCoef {}, opOut {};
        float fbPrev1 = 0, fbPrev2 = 0;
        float pitchEnv = 0, pitchCoef = 0;
        Butter4 aa;             // 2x decimation filter
        // transient
        double samplePos = 0;
        Crusher crusher;
        // noise
        Rng rng;
        float noiseEnv = 0, noiseCoef = 0, noiseLp = 0, noiseHp = 0;
        // shape
        Svf svf;
        int stage = 0;          // 0 attack, 1 hold, 2 decay
        float amp = 0, attackInc = 1, decayCoefAmp = 0;
        int holdLeft = 0;
        float chokeGain = 1.0f, chokeCoef = 1.0f;
        bool choking = false;
        float gainL = 1, gainR = 1;
        float meter = 0;
    };

    void startVoice (Voice& v, int pad, float velocity);
    void renderVoice (Voice& v, float* outL, float* outR, int n);
    float renderFm (Voice& v, double baseHz);

    double fs = 44100.0;
    int maxBlock = 512;
    uint64_t ageCounter = 0;
    std::array<VoiceParams, kNumPads> pads {};
    GlobalParams global;
    std::array<Voice, kMaxVoices> voices {};
    std::array<bool, kNumOutBuses> busActive {};
    std::array<std::vector<float>, kNumOutBuses * 2> buffers;
    std::array<Crusher, kNumOutBuses * 2> masterCrushers {};
    std::array<std::atomic<uint32_t>, kNumPads> triggerCount {};
    std::array<std::atomic<float>, kNumPads> padMeter {};
    uint32_t noiseSeed = 1;
};
} // namespace df
