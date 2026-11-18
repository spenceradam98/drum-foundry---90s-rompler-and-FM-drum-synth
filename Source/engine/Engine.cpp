#include "Engine.h"

#include <algorithm>
#include <cmath>

#include "Samples.h"

namespace df
{
const char* padName (int pad)
{
    static const char* names[kNumPads] = { "Kick", "Snare", "Clap", "Closed Hat", "Open Hat", "Tom", "Cymbal", "Perc" };
    return (pad >= 0 && pad < kNumPads) ? names[pad] : "";
}

const Algorithm& algorithm (int index)
{
    // Ops are 0..3 (shown as 1..4). mod[t][s] = op s modulates op t.
    static const Algorithm algos[kNumAlgorithms] = {
        { "Stack 4>3>2>1", {{ {0,1,0,0}, {0,0,1,0}, {0,0,0,1}, {0,0,0,0} }}, { 1,0,0,0 } },
        { "Branch (3+4)>2>1", {{ {0,1,0,0}, {0,0,1,1}, {0,0,0,0}, {0,0,0,0} }}, { 1,0,0,0 } },
        { "Y 4>3>1, 2>1", {{ {0,1,1,0}, {0,0,0,0}, {0,0,0,1}, {0,0,0,0} }}, { 1,0,0,0 } },
        { "Two pairs 2>1, 4>3", {{ {0,1,0,0}, {0,0,0,0}, {0,0,0,1}, {0,0,0,0} }}, { 1,0,1,0 } },
        { "Fan 4>(1,2,3)", {{ {0,0,0,1}, {0,0,0,1}, {0,0,0,1}, {0,0,0,0} }}, { 1,1,1,0 } },
        { "Stack 4>3>2 + 1", {{ {0,0,0,0}, {0,0,1,0}, {0,0,0,1}, {0,0,0,0} }}, { 1,1,0,0 } },
        { "One pair 4>3 + 1 + 2", {{ {0,0,0,0}, {0,0,0,0}, {0,0,0,1}, {0,0,0,0} }}, { 1,1,1,0 } },
        { "Additive 1+2+3+4", {{ {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0} }}, { 1,1,1,1 } },
    };
    return algos[std::clamp (index, 0, kNumAlgorithms - 1)];
}

int padForNote (int note, const std::array<int, kNumPads>& padNotes)
{
    for (int i = 0; i < kNumPads; ++i)
        if (padNotes[(size_t) i] == note) return i;
    switch (note)
    {
        case 35: return 0;                                         // acoustic bass drum -> kick
        case 40: return 1;                                         // electric snare -> snare
        case 44: return 3;                                         // pedal hat -> closed hat
        case 41: case 43: case 47: case 48: case 50: return 5;     // toms -> tom
        case 51: case 52: case 53: case 55: case 57: case 59: return 6;   // rides / crashes -> cymbal
        case 54: case 56: case 69: case 70: case 75: case 76: case 77: return 7;   // hand perc -> perc
        default: return -1;
    }
}

Engine::Engine()
{
    for (auto& b : busActive) b = false;
    busActive[0] = true;
    for (int i = 0; i < kNumPads; ++i) pads[(size_t) i].note = kDefaultNotes[(size_t) i];
    prepare (44100.0, 512);
}

void Engine::prepare (double sampleRate, int maxBlockSize)
{
    fs = sampleRate;
    maxBlock = std::max (1, maxBlockSize);
    for (auto& b : buffers) b.assign ((size_t) maxBlock, 0.0f);
    reset();
}

void Engine::reset()
{
    for (auto& v : voices) v.active = false;
    for (auto& c : masterCrushers) c.reset();
    for (auto& m : padMeter) m.store (0.0f);
}

void Engine::allNotesOff()
{
    for (auto& v : voices)
        if (v.active && ! v.choking) { v.choking = true; v.chokeCoef = decayCoef (20.0f, fs); }
}

int Engine::activeVoices() const
{
    int n = 0;
    for (auto& v : voices) n += v.active ? 1 : 0;
    return n;
}

void Engine::midiNote (int note, float velocity)
{
    std::array<int, kNumPads> notes {};
    for (int i = 0; i < kNumPads; ++i) notes[(size_t) i] = pads[(size_t) i].note;
    const int pad = padForNote (note, notes);
    if (pad >= 0) noteOn (pad, velocity);
}

void Engine::noteOn (int pad, float velocity)
{
    if (pad < 0 || pad >= kNumPads) return;
    velocity = clampf (velocity, 0.0f, 1.0f);
    const int group = pads[(size_t) pad].chokeGroup;

    // choke: same pad retriggers, and every pad in the same choke group
    for (auto& v : voices)
        if (v.active && ! v.choking && (v.pad == pad || (group != 0 && v.p.chokeGroup == group)))
        {
            v.choking = true;
            v.chokeCoef = decayCoef (5.0f, fs);
        }

    Voice* slot = nullptr;
    for (auto& v : voices) if (! v.active) { slot = &v; break; }
    if (slot == nullptr)   // steal the oldest voice
    {
        slot = &voices[0];
        for (auto& v : voices) if (v.age < slot->age) slot = &v;
    }
    startVoice (*slot, pad, velocity);
    triggerCount[(size_t) pad].fetch_add (1);
}

void Engine::startVoice (Voice& v, int pad, float velocity)
{
    const auto& p = pads[(size_t) pad];
    v = Voice {};
    v.active = true;
    v.pad = pad;
    v.age = ++ageCounter;
    v.velocity = velocity;
    v.p = p;

    const double fs2 = fs * 2.0;
    const float decayScale = 0.7f + 0.3f * velocity;
    for (int i = 0; i < kNumOps; ++i)
    {
        v.opEnv[(size_t) i] = 1.0f;
        v.opCoef[(size_t) i] = decayCoef (p.op[(size_t) i].decayMs * decayScale, fs2);
    }
    v.pitchEnv = 1.0f;
    v.pitchCoef = decayCoef (std::max (1.0f, p.pitchEnvMs), fs);
    v.aa.set (fs2, std::min (20000.0, fs * 0.45));

    const auto& set = transientSet();
    if (p.sample >= 0 && p.sample < (int) set.size())
        v.samplePos = clampf (p.sampleStart, 0.0f, 1.0f) * (double) set[(size_t) p.sample].data.size();

    v.rng = Rng (0x5eed1234u + (noiseSeed++ * 2654435761u));
    v.noiseEnv = 1.0f;
    v.noiseCoef = decayCoef (p.noiseDecayMs * decayScale, fs);

    v.svf.set (fs, p.cutoffHz, p.resonance);

    const int attackSamples = (int) (p.attackMs * 0.001 * fs);
    if (attackSamples > 0) { v.stage = 0; v.amp = 0.0f; v.attackInc = 1.0f / (float) attackSamples; }
    else { v.stage = 1; v.amp = 1.0f; }
    v.holdLeft = (int) (p.holdMs * 0.001 * fs);
    v.decayCoefAmp = decayCoef (std::max (1.0f, p.decayMs * decayScale), fs);

    const float velGain = std::pow (velocity, 1.3f);
    const float g = dbToGain (p.levelDb) * velGain;
    const float angle = (clampf (p.pan, -1.0f, 1.0f) + 1.0f) * 0.25f * (float) kPi;
    v.gainL = g * std::cos (angle) * 1.41421356f;
    v.gainR = g * std::sin (angle) * 1.41421356f;
}

float Engine::renderFm (Voice& v, double baseHz)
{
    const auto& al = algorithm (v.p.algorithm);
    const auto& sine = SineTable::get();
    const double fs2 = fs * 2.0;
    const float idxScale = 0.5f + 0.5f * v.velocity;
    constexpr float kMaxIndexCycles = 1.1f;   // ~7 rad of modulation at level 1

    std::array<float, kNumOps> modOut {};
    float carrierSum = 0.0f;
    int carriers = 0;
    for (int o = kNumOps - 1; o >= 0; --o)
    {
        const size_t oi = (size_t) o;
        float pm = 0.0f;
        for (int s = o + 1; s < kNumOps; ++s)
            if (al.mod[oi][(size_t) s]) pm += modOut[(size_t) s];
        if (o == kNumOps - 1 && v.p.feedback > 0.0f)
            pm += v.p.feedback * 0.35f * 0.5f * (v.fbPrev1 + v.fbPrev2);

        const float raw = sine (v.phase[oi] + pm) * v.opEnv[oi];
        if (o == kNumOps - 1) { v.fbPrev2 = v.fbPrev1; v.fbPrev1 = raw; }
        const float lvl = v.p.op[oi].level;
        if (al.carrier[oi]) { carrierSum += raw * lvl; ++carriers; }
        else modOut[oi] = raw * lvl * idxScale * kMaxIndexCycles;

        v.phase[oi] += baseHz * v.p.op[oi].ratio / fs2;
        if (v.phase[oi] >= 1.0) v.phase[oi] -= std::floor (v.phase[oi]);
        v.opEnv[oi] *= v.opCoef[oi];
    }
    return carriers > 0 ? carrierSum / std::sqrt ((float) carriers) : 0.0f;
}

void Engine::renderVoice (Voice& v, float* outL, float* outR, int n)
{
    const auto& p = v.p;
    const auto& set = transientSet();
    const OneShot* shot = (p.sampleLevel > 0.0f && p.sample >= 0 && p.sample < (int) set.size()) ? &set[(size_t) p.sample] : nullptr;
    const double sampleInc = (kTransientRate / fs) * std::pow (2.0, p.samplePitch / 12.0);
    const double holdHz = clampf (p.sampleRateKHz, 8.0f, 44.1f) * 1000.0;
    const float bits = clampf (p.sampleBits, 4.0f, 16.0f);
    const bool fmOn = p.fmLevel > 0.0f;
    const bool noiseOn = p.noiseLevel > 0.0f;

    // noise colour: one-pole low-pass (dark) or high-pass (bright)
    const float colour = clampf (p.noiseColour, -1.0f, 1.0f);
    const float lpHz = colour < 0 ? 20000.0f * std::pow (0.025f, -colour) : 20000.0f;
    const float hpHz = colour > 0 ? 20.0f * std::pow (400.0f, colour) : 20.0f;
    const float lpA = (float) std::exp (-kTwoPi * std::min ((double) lpHz, fs * 0.49) / fs);
    const float hpA = (float) std::exp (-kTwoPi * hpHz / fs);

    const float driveGain = 1.0f + p.drive * 8.0f;
    const float driveNorm = p.drive > 0.001f ? 1.0f / std::tanh (driveGain) : 1.0f;
    const int mode = (int) p.filter;
    float peak = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        float x = 0.0f;
        if (fmOn)
        {
            const double hz = p.pitchHz * std::pow (2.0, p.pitchEnvSemis * v.pitchEnv / 12.0);
            v.aa.process (renderFm (v, hz));
            x += v.aa.process (renderFm (v, hz)) * p.fmLevel;
            v.pitchEnv *= v.pitchCoef;
        }
        if (shot != nullptr)
        {
            const auto len = (double) shot->data.size();
            const float s = v.crusher.tick (holdHz, fs, bits, [&] {
                if (v.samplePos >= len - 1) return 0.0f;
                const int k = (int) v.samplePos;
                const float f = (float) (v.samplePos - k);
                const float a = shot->data[(size_t) k] / 32768.0f, b = shot->data[(size_t) k + 1] / 32768.0f;
                return a + f * (b - a);
            });
            v.samplePos += sampleInc;
            x += s * p.sampleLevel;
        }
        if (noiseOn)
        {
            float w = v.rng.bipolar();
            v.noiseLp = w + lpA * (v.noiseLp - w);
            v.noiseHp = v.noiseLp + hpA * (v.noiseHp - v.noiseLp);
            w = v.noiseLp - v.noiseHp;
            x += w * p.noiseLevel * v.noiseEnv;
            v.noiseEnv *= v.noiseCoef;
        }
        if (p.drive > 0.001f) x = std::tanh (x * driveGain) * driveNorm;
        x = v.svf.process (x, mode);

        // amp AHD
        if (v.stage == 0) { v.amp += v.attackInc; if (v.amp >= 1.0f) { v.amp = 1.0f; v.stage = 1; } }
        else if (v.stage == 1) { if (--v.holdLeft <= 0) v.stage = 2; }
        else v.amp *= v.decayCoefAmp;
        if (v.choking) v.chokeGain *= v.chokeCoef;

        const float y = x * v.amp * v.chokeGain;
        outL[i] += y * v.gainL;
        outR[i] += y * v.gainR;
        peak = std::max (peak, v.amp * v.chokeGain);

        if ((v.stage == 2 && v.amp < 1.0e-4f) || v.chokeGain < 1.0e-4f) { v.active = false; break; }
    }
    v.meter = peak * v.velocity;
}

void Engine::process (int n)
{
    n = std::min (n, maxBlock);
    for (auto& b : buffers) std::fill (b.begin(), b.begin() + n, 0.0f);

    std::array<float, kNumPads> meters {};
    for (auto& v : voices)
    {
        if (! v.active) continue;
        int b = std::clamp (v.p.outBus, 0, kNumOutBuses - 1);
        if (! busActive[(size_t) b]) b = 0;
        renderVoice (v, buffers[(size_t) b * 2].data(), buffers[(size_t) b * 2 + 1].data(), n);
        meters[(size_t) v.pad] = std::max (meters[(size_t) v.pad], v.meter);
    }
    const float meterFall = (float) std::exp (-(double) n / (0.12 * fs));
    for (int i = 0; i < kNumPads; ++i)
        padMeter[(size_t) i].store (std::max (meters[(size_t) i], padMeter[(size_t) i].load() * meterFall));

    const float crush = clampf (global.masterCrush, 0.0f, 1.0f);
    const float out = dbToGain (global.outputDb);
    const float bits = 16.0f - 10.0f * crush;
    const double holdHz = 44100.0 * (1.0 - 0.72 * crush);
    for (size_t c = 0; c < buffers.size(); ++c)
    {
        auto* d = buffers[c].data();
        if (crush > 0.001f)
            for (int i = 0; i < n; ++i)
            {
                const float in = d[i];
                d[i] = masterCrushers[c].tick (holdHz, fs, bits, [in] { return in; });
            }
        for (int i = 0; i < n; ++i) d[i] *= out;
    }
}
} // namespace df
