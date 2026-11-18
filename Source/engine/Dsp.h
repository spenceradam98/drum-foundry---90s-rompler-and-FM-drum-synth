// Small scalar DSP building blocks for the drum-foundry engine (no JUCE, no SIMD).
#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace df
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

inline float dbToGain (float db) { return db <= -79.9f ? 0.0f : std::pow (10.0f, db / 20.0f); }
inline float clampf (float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

// Per-sample multiplier that makes an exponential decay fall by 60 dB in `ms`.
inline float decayCoef (float ms, double fs)
{
    if (ms <= 0.0f) return 0.0f;
    return (float) std::exp (std::log (0.001) / (ms * 0.001 * fs));
}

// 4096-point sine table with linear interpolation. Phase is in cycles (0..1).
struct SineTable
{
    static constexpr int N = 4096;
    std::array<float, N + 1> t {};
    SineTable() { for (int i = 0; i <= N; ++i) t[(size_t) i] = (float) std::sin (kTwoPi * i / N); }
    float operator() (double phaseCycles) const
    {
        double p = phaseCycles - std::floor (phaseCycles);
        const double x = p * N;
        const int i = (int) x;
        const float f = (float) (x - i);
        return t[(size_t) i] + f * (t[(size_t) i + 1] - t[(size_t) i]);
    }
    static const SineTable& get() { static const SineTable s; return s; }
};

// Deterministic xorshift noise, -1..1.
struct Rng
{
    uint32_t s = 0x9e3779b9u;
    Rng (uint32_t seed = 0x9e3779b9u) : s (seed ? seed : 1u) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float bipolar() { return (float) ((double) next() / 2147483648.0 - 1.0); }
    float unipolar() { return (float) ((double) next() / 4294967296.0); }
};

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void setLowpass (double fs, double fc, double q)
    {
        fc = std::fmin (fc, fs * 0.49);
        const double w = kTwoPi * fc / fs, cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (1.0 - cw) * 0.5 / a0; b1 = (1.0 - cw) / a0; b2 = b0;
        a1 = -2.0 * cw / a0; a2 = (1.0 - alpha) / a0;
    }
    void reset() { z1 = z2 = 0; }
    float process (float x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }
};

// 4-pole Butterworth low-pass: used as the 2x oversampling decimation filter.
struct Butter4
{
    Biquad s1, s2;
    void set (double fs, double fc) { s1.setLowpass (fs, fc, 0.5412); s2.setLowpass (fs, fc, 1.3066); }
    void reset() { s1.reset(); s2.reset(); }
    float process (float x) { return s2.process (s1.process (x)); }
};

// Topology-preserving state-variable filter (LP / BP / HP).
struct Svf
{
    float ic1 = 0, ic2 = 0;
    float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0;
    void set (double fs, float fc, float reso /*0..1*/)
    {
        fc = (float) std::fmin (std::fmax (fc, 20.0f), fs * 0.45);
        g = (float) std::tan (kPi * fc / fs);
        k = 2.0f - 1.96f * clampf (reso, 0.0f, 1.0f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }
    // mode 0 = LP, 1 = BP, 2 = HP
    float process (float x, int mode)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        if (mode == 0) return v2;
        if (mode == 1) return v1 * k;
        return x - k * v1 - v2;
    }
};

// Zero-order-hold "DAC" with n-bit quantisation: the same idea as crate12's 12-bit / 26 kHz
// converter, with the bit depth and hold rate made variable.
struct Crusher
{
    double phase = 1.0;   // >= 1 forces a fresh sample on the first call
    float held = 0;
    void reset() { phase = 1.0; held = 0; }
    static float quantise (float x, float bits)
    {
        if (bits >= 15.99f) return x;
        const float steps = std::pow (2.0f, bits - 1.0f);
        return std::round (clampf (x, -1.0f, 1.0f) * steps) / steps;
    }
    // `in` is evaluated only when the hold clock ticks; returns the held, quantised value.
    template <typename Fn>
    float tick (double holdHz, double fs, float bits, Fn&& in)
    {
        phase += holdHz / fs;
        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            held = quantise (in(), bits);
        }
        return held;
    }
};
} // namespace df
