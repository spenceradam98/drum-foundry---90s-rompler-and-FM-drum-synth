#include "Presets.h"

#include "Engine.h"

namespace df
{
namespace
{
// Built-in transient indices (see Samples.cpp)
enum Smp { Thump, Click, Snap, Rim, NoiseBurst, ClapMulti, Shaker, MetalTick, Wood, Zap, CrashWash, VinylPop, TapeHit, BellPing, Stick, Breath };

VoiceParams base (int pad)
{
    VoiceParams v;
    v.note = kDefaultNotes[(size_t) pad];
    v.op[0] = { 1.0f, 1.0f, 300.0f };
    for (int i = 1; i < kNumOps; ++i) v.op[(size_t) i] = { 1.0f, 0.0f, 100.0f };
    return v;
}

void crunch (VoiceParams& v, float bits, float khz) { v.sampleBits = bits; v.sampleRateKHz = khz; }

VoiceParams kick (float hz, float sweep, float sweepMs, float decay, float click, float drive, int smp = Click)
{
    auto v = base (0);
    v.algorithm = 0;
    v.op[0] = { 1.0f, 1.0f, decay };
    v.op[1] = { 1.0f, 0.18f, 40.0f };   // a touch of FM thump at the start
    v.pitchHz = hz; v.pitchEnvSemis = sweep; v.pitchEnvMs = sweepMs;
    v.sample = smp; v.sampleLevel = click; crunch (v, 12, 26);
    v.cutoffHz = 9000; v.decayMs = decay * 1.2f; v.holdMs = 10; v.drive = drive;
    v.levelDb = -1.0f;
    return v;
}

VoiceParams snare (float hz, float noise, float noiseDecay, float colour, float body, int smp = Snap, float smpLevel = 0.5f)
{
    auto v = base (1);
    v.algorithm = 3;   // two pairs: ringing body + detuned overtone
    v.op[0] = { 1.0f, body, 120.0f };
    v.op[1] = { 1.48f, 0.35f, 60.0f };
    v.op[2] = { 1.62f, body * 0.6f, 80.0f };
    v.op[3] = { 2.3f, 0.25f, 40.0f };
    v.pitchHz = hz; v.pitchEnvSemis = 7; v.pitchEnvMs = 25;
    v.noiseLevel = noise; v.noiseDecayMs = noiseDecay; v.noiseColour = colour;
    v.sample = smp; v.sampleLevel = smpLevel; crunch (v, 12, 26);
    v.filter = FilterType::HighPass; v.cutoffHz = 120; v.decayMs = noiseDecay * 1.4f; v.holdMs = 5;
    v.levelDb = -3.0f;
    return v;
}

VoiceParams clap (float decay, float colour, float smpLevel, float noise)
{
    auto v = base (2);
    v.fmLevel = 0.0f;
    v.sample = ClapMulti; v.sampleLevel = smpLevel; crunch (v, 12, 26);
    v.noiseLevel = noise; v.noiseColour = colour; v.noiseDecayMs = decay;
    v.filter = FilterType::BandPass; v.cutoffHz = 1300; v.resonance = 0.25f;
    v.holdMs = 20; v.decayMs = decay * 1.3f;
    v.levelDb = -3.0f; v.pan = 0.1f;
    return v;
}

// Metallic hats: six-operator-style inharmonic FM squeezed into the "fan" and "pairs" algorithms.
VoiceParams hat (int pad, float hz, float decay, float cutoff, float smpLevel, int smp = MetalTick)
{
    auto v = base (pad);
    v.algorithm = 3;
    v.op[0] = { 1.0f, 0.8f, decay };
    v.op[1] = { 1.4142f, 0.9f, decay };
    v.op[2] = { 2.7183f, 0.7f, decay };
    v.op[3] = { 3.8f, 0.9f, decay };
    v.feedback = 0.4f;
    v.pitchHz = hz;
    v.noiseLevel = 0.35f; v.noiseColour = 0.6f; v.noiseDecayMs = decay * 0.8f;
    v.sample = smp; v.sampleLevel = smpLevel; crunch (v, 10, 22);
    v.filter = FilterType::HighPass; v.cutoffHz = cutoff; v.resonance = 0.15f;
    v.holdMs = 2; v.decayMs = decay;
    v.chokeGroup = 1;
    v.levelDb = -7.0f; v.pan = pad == 3 ? 0.25f : 0.3f;
    return v;
}

VoiceParams tom (float hz, float sweep, float decay, float smpLevel = 0.2f)
{
    auto v = base (5);
    v.algorithm = 1;
    v.op[0] = { 1.0f, 1.0f, decay };
    v.op[1] = { 1.5f, 0.12f, 60.0f };
    v.op[2] = { 2.01f, 0.08f, 30.0f };
    v.pitchHz = hz; v.pitchEnvSemis = sweep; v.pitchEnvMs = 60;
    v.sample = Stick; v.sampleLevel = smpLevel; crunch (v, 12, 26);
    v.noiseLevel = 0.05f; v.noiseColour = -0.3f; v.noiseDecayMs = 60;
    v.cutoffHz = 7000; v.holdMs = 5; v.decayMs = decay * 1.2f;
    v.levelDb = -3.0f; v.pan = -0.3f;
    return v;
}

VoiceParams cymbal (float hz, float decay, float smpLevel, float cutoff = 3500)
{
    auto v = base (6);
    v.algorithm = 4;
    v.op[0] = { 1.0f, 0.5f, decay };
    v.op[1] = { 1.73f, 0.5f, decay };
    v.op[2] = { 2.41f, 0.5f, decay };
    v.op[3] = { 3.13f, 0.95f, decay };
    v.feedback = 0.6f;
    v.pitchHz = hz;
    v.noiseLevel = 0.4f; v.noiseColour = 0.4f; v.noiseDecayMs = decay;
    v.sample = CrashWash; v.sampleLevel = smpLevel; crunch (v, 12, 26);
    v.filter = FilterType::HighPass; v.cutoffHz = cutoff; v.resonance = 0.1f;
    v.holdMs = 5; v.decayMs = decay;
    v.levelDb = -8.0f; v.pan = -0.2f;
    return v;
}

VoiceParams perc (float hz, float ratio, float index, float decay, int smp, float smpLevel)
{
    auto v = base (7);
    v.algorithm = 0;
    v.op[0] = { 1.0f, 1.0f, decay };
    v.op[1] = { ratio, index, decay * 0.6f };
    v.pitchHz = hz; v.pitchEnvSemis = 2; v.pitchEnvMs = 10;
    v.sample = smp; v.sampleLevel = smpLevel; crunch (v, 10, 22);
    v.cutoffHz = 12000; v.holdMs = 2; v.decayMs = decay * 1.2f;
    v.levelDb = -5.0f; v.pan = 0.35f;
    return v;
}

VoiceParams zap (float hz, float sweep, float decay)
{
    auto v = perc (hz, 2.0f, 0.5f, decay, Zap, 0.2f);
    v.pitchEnvSemis = sweep; v.pitchEnvMs = decay * 0.4f;
    v.feedback = 0.0f;
    return v;
}

// Applies a kit-wide crunch: every pad's transient layer gets the same converter.
void kitCrunch (Kit& k, float bits, float khz)
{
    for (auto& p : k.pads) crunch (p, bits, khz);
}

std::vector<Kit> buildKits()
{
    std::vector<Kit> kits;
    auto add = [&] (const char* name, std::array<VoiceParams, kNumPads> pads) {
        Kit k { name, pads };
        for (int i = 0; i < kNumPads; ++i) k.pads[(size_t) i].note = kDefaultNotes[(size_t) i];
        kits.push_back (k);
        return &kits.back();
    };

    add ("909-ish House", { kick (52, 30, 22, 420, 0.35f, 0.15f), snare (190, 0.6f, 180, 0.2f, 0.5f),
                            clap (260, 0.1f, 0.8f, 0.25f), hat (3, 420, 55, 7000, 0.3f), hat (4, 420, 320, 6500, 0.3f),
                            tom (110, 7, 320), cymbal (380, 1400, 0.5f), perc (520, 3.0f, 0.4f, 90, Rim, 0.5f) });
    kitCrunch (*add ("Rave Metal", { kick (48, 36, 18, 380, 0.4f, 0.55f, Thump), snare (210, 0.55f, 160, 0.5f, 0.4f, Snap, 0.6f),
                                     clap (220, 0.3f, 0.7f, 0.3f), hat (3, 560, 45, 8000, 0.4f), hat (4, 560, 260, 7500, 0.4f),
                                     zap (180, 30, 260), cymbal (520, 1100, 0.3f, 4200), perc (880, 3.5f, 0.8f, 120, BellPing, 0.3f) }),
               10, 22);
    kitCrunch (*add ("Jungle Crunch", { kick (58, 18, 30, 280, 0.5f, 0.3f, TapeHit), snare (240, 0.45f, 140, 0.3f, 0.6f, Snap, 0.8f),
                                        clap (200, 0.2f, 0.6f, 0.2f), hat (3, 470, 40, 6500, 0.6f, Shaker), hat (4, 470, 220, 6000, 0.5f, Shaker),
                                        tom (160, 5, 200, 0.4f), cymbal (430, 900, 0.6f), perc (610, 2.0f, 0.3f, 80, Rim, 0.7f) }),
               8, 16);
    add ("Electro Zap", { kick (45, 40, 35, 520, 0.2f, 0.25f), snare (180, 0.75f, 200, 0.7f, 0.3f, NoiseBurst, 0.3f),
                          clap (300, 0.5f, 0.6f, 0.4f), hat (3, 610, 35, 9000, 0.2f), hat (4, 610, 200, 8500, 0.2f),
                          zap (220, 36, 300), cymbal (600, 1200, 0.2f, 5000), zap (900, 24, 140) });
    kitCrunch (*add ("Trip-Hop Dust", { kick (55, 14, 40, 360, 0.6f, 0.2f, TapeHit), snare (170, 0.4f, 220, -0.4f, 0.5f, Snap, 0.7f),
                                        clap (320, -0.2f, 0.5f, 0.2f), hat (3, 380, 60, 4500, 0.6f, Shaker), hat (4, 380, 280, 4200, 0.5f, Shaker),
                                        tom (95, 4, 380, 0.3f), cymbal (330, 1600, 0.7f, 2500), perc (300, 1.0f, 0.2f, 140, VinylPop, 0.9f) }),
               8, 11);
    add ("Industrial FM", { kick (42, 30, 25, 460, 0.5f, 0.8f, Thump), snare (150, 0.7f, 260, 0.1f, 0.6f, TapeHit, 0.6f),
                            clap (260, 0.0f, 0.7f, 0.5f), hat (3, 700, 70, 6000, 0.5f), hat (4, 700, 420, 5500, 0.5f),
                            tom (70, 12, 420, 0.5f), cymbal (290, 2200, 0.6f, 2000), perc (440, 7.1f, 1.0f, 200, MetalTick, 0.6f) });
    kitCrunch (*add ("Big Beat", { kick (60, 20, 30, 340, 0.6f, 0.6f, TapeHit), snare (230, 0.6f, 200, 0.2f, 0.6f, Snap, 0.8f),
                                   clap (280, 0.1f, 0.8f, 0.3f), hat (3, 450, 55, 6000, 0.6f, Shaker), hat (4, 450, 300, 5500, 0.6f, Shaker),
                                   tom (130, 6, 300, 0.4f), cymbal (400, 1500, 0.8f, 3000), perc (760, 2.0f, 0.3f, 90, Stick, 0.7f) }),
               10, 16);
    kitCrunch (*add ("Lo-Fi Toy", { kick (70, 12, 20, 200, 0.3f, 0.1f, Wood), snare (300, 0.4f, 110, 0.4f, 0.3f, Stick, 0.4f),
                                    clap (160, 0.5f, 0.5f, 0.2f), hat (3, 800, 30, 7000, 0.3f), hat (4, 800, 150, 7000, 0.3f),
                                    tom (220, 3, 160, 0.2f), cymbal (700, 700, 0.3f, 5000), perc (1200, 1.0f, 0.1f, 60, BellPing, 0.5f) }),
               6, 8);
    add ("Deep Garage", { kick (50, 24, 28, 500, 0.2f, 0.1f), snare (200, 0.5f, 150, 0.0f, 0.5f, Rim, 0.5f),
                          clap (240, -0.1f, 0.7f, 0.2f), hat (3, 500, 45, 7500, 0.2f, Shaker), hat (4, 500, 230, 7000, 0.2f, Shaker),
                          tom (120, 6, 260), cymbal (450, 1200, 0.4f, 4000), perc (640, 1.5f, 0.2f, 70, Rim, 0.6f) });
    add ("Acid Tribal", { kick (54, 28, 26, 380, 0.3f, 0.35f), snare (260, 0.35f, 120, -0.2f, 0.7f, Wood, 0.5f),
                          clap (200, 0.0f, 0.6f, 0.25f), hat (3, 520, 50, 6500, 0.4f, Shaker), hat (4, 520, 260, 6000, 0.4f, Shaker),
                          tom (180, 8, 260, 0.3f), cymbal (480, 900, 0.3f, 3500), perc (380, 2.0f, 0.6f, 160, Wood, 0.6f) });
    kitCrunch (*add ("Hardcore Gabba", { kick (62, 40, 40, 300, 0.4f, 1.0f, Thump), snare (220, 0.8f, 150, 0.5f, 0.4f, NoiseBurst, 0.6f),
                                         clap (200, 0.4f, 0.8f, 0.4f), hat (3, 650, 40, 8500, 0.4f), hat (4, 650, 220, 8000, 0.4f),
                                         zap (150, 40, 220), cymbal (560, 1000, 0.5f, 4500), perc (990, 3.0f, 0.9f, 100, MetalTick, 0.5f) }),
               10, 22);
    add ("Minimal Click", { kick (56, 18, 18, 260, 0.3f, 0.0f, Click), snare (330, 0.2f, 70, 0.6f, 0.3f, Click, 0.6f),
                            clap (150, 0.6f, 0.5f, 0.1f), hat (3, 900, 25, 10000, 0.3f, Click), hat (4, 900, 140, 9500, 0.3f),
                            tom (250, 2, 120, 0.4f), cymbal (900, 600, 0.2f, 7000), perc (2000, 1.0f, 0.0f, 30, Click, 0.8f) });
    add ("Boom Bap Chrome", { kick (58, 16, 30, 330, 0.6f, 0.3f, TapeHit), snare (210, 0.5f, 190, -0.1f, 0.6f, Snap, 0.9f),
                              clap (260, 0.0f, 0.7f, 0.2f), hat (3, 430, 50, 5500, 0.6f, Shaker), hat (4, 430, 260, 5000, 0.6f, Shaker),
                              tom (140, 5, 260, 0.4f), cymbal (370, 1300, 0.7f, 3000), perc (520, 1.0f, 0.1f, 80, VinylPop, 0.8f) });
    add ("Dub Cavern", { kick (46, 12, 40, 600, 0.2f, 0.1f), snare (160, 0.4f, 300, -0.5f, 0.6f, Rim, 0.6f),
                         clap (380, -0.4f, 0.5f, 0.3f), hat (3, 360, 70, 4000, 0.3f, Shaker), hat (4, 360, 360, 3800, 0.3f, Shaker),
                         tom (90, 10, 500, 0.2f), cymbal (300, 2000, 0.4f, 2500), perc (240, 1.0f, 0.2f, 300, Wood, 0.5f) });
    add ("Breakbeat Steel", { kick (57, 22, 25, 320, 0.5f, 0.4f, Thump), snare (250, 0.55f, 170, 0.3f, 0.6f, Snap, 0.7f),
                              clap (230, 0.2f, 0.7f, 0.3f), hat (3, 540, 45, 7000, 0.5f), hat (4, 540, 260, 6500, 0.5f),
                              tom (150, 6, 280, 0.4f), cymbal (460, 1400, 0.6f, 3500), perc (700, 4.0f, 0.5f, 110, MetalTick, 0.4f) });
    kitCrunch (*add ("Game Console", { kick (65, 30, 15, 220, 0.2f, 0.0f, Zap), snare (280, 0.7f, 100, 0.8f, 0.2f, NoiseBurst, 0.4f),
                                       clap (150, 0.8f, 0.4f, 0.4f), hat (3, 1000, 25, 9000, 0.2f), hat (4, 1000, 140, 9000, 0.2f),
                                       zap (300, 24, 160), cymbal (800, 600, 0.3f, 6000), zap (1200, 12, 80) }),
               4, 8);
    return kits;
}

std::vector<VoicePreset> buildVoicePresets()
{
    return {
        { "Kick: Round 909-ish", kick (52, 30, 22, 420, 0.35f, 0.15f) },
        { "Kick: Rave Boom", kick (48, 36, 18, 380, 0.4f, 0.55f, Thump) },
        { "Kick: Distorted Gabba", kick (62, 40, 40, 300, 0.4f, 1.0f, Thump) },
        { "Kick: Long Sub", kick (46, 12, 40, 900, 0.1f, 0.0f) },
        { "Kick: Toy Thud", kick (70, 12, 20, 200, 0.3f, 0.1f, Wood) },
        { "Snare: House", snare (190, 0.6f, 180, 0.2f, 0.5f) },
        { "Snare: Jungle Crack", snare (240, 0.45f, 140, 0.3f, 0.6f, Snap, 0.8f) },
        { "Snare: Electro Noise", snare (180, 0.75f, 200, 0.7f, 0.3f, NoiseBurst, 0.3f) },
        { "Snare: Rim Shot", snare (330, 0.2f, 70, 0.6f, 0.3f, Rim, 0.8f) },
        { "Clap: Classic", clap (260, 0.1f, 0.8f, 0.25f) },
        { "Clap: Big Room", clap (420, 0.0f, 0.8f, 0.4f) },
        { "Hat: Closed Metal", hat (3, 420, 55, 7000, 0.3f) },
        { "Hat: Open Metal", hat (4, 420, 320, 6500, 0.3f) },
        { "Hat: Shaker Closed", hat (3, 470, 40, 6500, 0.6f, Shaker) },
        { "Tom: Low FM", tom (90, 10, 500, 0.2f) },
        { "Tom: High Snappy", tom (200, 6, 200, 0.4f) },
        { "Cymbal: Crash", cymbal (380, 1400, 0.5f) },
        { "Cymbal: Ride Bell", cymbal (700, 1800, 0.1f, 5000) },
        { "Perc: FM Bell", perc (880, 3.5f, 0.8f, 300, BellPing, 0.3f) },
        { "Perc: Cowbell-ish", perc (560, 1.48f, 0.6f, 160, MetalTick, 0.2f) },
        { "Perc: Wood Block", perc (380, 2.0f, 0.6f, 120, Wood, 0.6f) },
        { "Perc: Laser Zap", zap (220, 36, 300) },
        { "Perc: Vinyl Pop", perc (300, 1.0f, 0.2f, 140, VinylPop, 0.9f) },
        { "Perc: Clave", perc (2500, 1.0f, 0.0f, 50, Stick, 0.5f) },
    };
}
} // namespace

const std::vector<Kit>& factoryKits()
{
    static const std::vector<Kit> k = buildKits();
    return k;
}

const std::vector<VoicePreset>& voicePresets()
{
    static const std::vector<VoicePreset> v = buildVoicePresets();
    return v;
}
} // namespace df
