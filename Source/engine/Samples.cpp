#include "Samples.h"

#include <algorithm>
#include <cmath>
#include <functional>

#include "Dsp.h"

namespace df
{
namespace
{
constexpr double R = kTransientRate;

// Renders `ms` milliseconds with fn(t seconds, rng) and stores it normalised to -1 dBFS, 16-bit.
OneShot render (const char* name, double ms, uint32_t seed, const std::function<double (double, Rng&)>& fn)
{
    const int n = (int) (ms * 0.001 * R);
    std::vector<double> x ((size_t) n);
    Rng rng (seed);
    double peak = 1.0e-9;
    for (int i = 0; i < n; ++i)
    {
        x[(size_t) i] = fn (i / R, rng);
        peak = std::max (peak, std::abs (x[(size_t) i]));
    }
    OneShot s { name, std::vector<int16_t> ((size_t) n) };
    const double g = 0.891 / peak;
    for (int i = 0; i < n; ++i)
    {
        // short fade-out on the last 2 ms so the tail never clicks
        const double fade = std::min (1.0, (n - 1 - i) / (0.002 * R));
        s.data[(size_t) i] = (int16_t) std::lround (std::clamp (x[(size_t) i] * g * fade, -1.0, 1.0) * 32767.0);
    }
    return s;
}

double env (double t, double ms) { return std::exp (-t / (ms * 0.001) * 6.9078); }   // -60 dB at `ms`

// One-pole filters held in lambdas' captured state
struct OnePole
{
    double z = 0, a = 0;
    OnePole (double hz) : a (std::exp (-kTwoPi * hz / R)) {}
    double lp (double x) { z = x + a * (z - x); return z; }
    double hp (double x) { return x - lp (x); }
};

std::vector<OneShot> build()
{
    std::vector<OneShot> v;

    v.push_back (render ("Thump", 260, 11, [] (double t, Rng&) {
        const double f = 48.0 + 180.0 * env (t, 40);
        static double ph; if (t == 0) ph = 0; ph += f / R;
        return std::sin (kTwoPi * ph) * env (t, 250);
    }));
    v.push_back (render ("Click", 18, 12, [] (double t, Rng& r) {
        return (r.bipolar() * 0.6 + std::sin (kTwoPi * 3200 * t)) * env (t, 12);
    }));
    v.push_back (render ("Snap", 140, 13, [lp = OnePole (6000), hp = OnePole (900)] (double t, Rng& r) mutable {
        const double body = std::sin (kTwoPi * 215 * t) * env (t, 50) * 0.8;
        return body + hp.hp (lp.lp (r.bipolar())) * env (t, 120);
    }));
    v.push_back (render ("Rim", 60, 14, [] (double t, Rng&) {
        return (std::sin (kTwoPi * 1670 * t) + 0.6 * std::sin (kTwoPi * 495 * t)) * env (t, 45);
    }));
    v.push_back (render ("Noise Burst", 220, 15, [hp = OnePole (300)] (double t, Rng& r) mutable {
        return hp.hp (r.bipolar()) * env (t, 200);
    }));
    v.push_back (render ("Clap Multi", 300, 16, [lp = OnePole (5000), hp = OnePole (700)] (double t, Rng& r) mutable {
        // three short bursts 10 ms apart, then a longer tail
        double e = 0;
        for (int k = 0; k < 3; ++k) { const double tk = t - k * 0.010; if (tk >= 0) e = std::max (e, env (tk, 9)); }
        if (t >= 0.03) e = std::max (e, 0.7 * env (t - 0.03, 260));
        return hp.hp (lp.lp (r.bipolar())) * e;
    }));
    v.push_back (render ("Shaker", 120, 17, [hp = OnePole (4500)] (double t, Rng& r) mutable {
        const double a = std::min (1.0, t / 0.02);
        return hp.hp (r.bipolar()) * a * env (t, 110);
    }));
    v.push_back (render ("Metal Tick", 90, 18, [] (double t, Rng&) {
        static const double fr[] = { 2950, 4213, 5701, 7177, 8939, 10441 };
        double s = 0; for (double f : fr) s += (std::sin (kTwoPi * f * t) > 0 ? 1.0 : -1.0);
        return s / 6.0 * env (t, 80);
    }));
    v.push_back (render ("Wood", 110, 19, [] (double t, Rng&) {
        return (std::sin (kTwoPi * 820 * t) + 0.4 * std::sin (kTwoPi * 2260 * t)) * env (t, 90);
    }));
    v.push_back (render ("Zap", 160, 20, [] (double t, Rng&) {
        static double ph; if (t == 0) ph = 0; ph += (90.0 + 2400.0 * env (t, 60)) / R;
        return std::sin (kTwoPi * ph) * env (t, 150);
    }));
    v.push_back (render ("Crash Wash", 900, 21, [hp = OnePole (2500), lp = OnePole (12000)] (double t, Rng& r) mutable {
        static const double fr[] = { 3311, 4523, 6207, 7919 };
        double m = 0; for (double f : fr) m += std::sin (kTwoPi * f * t);
        return (hp.hp (lp.lp (r.bipolar())) + 0.15 * m) * env (t, 880);
    }));
    v.push_back (render ("Vinyl Pop", 30, 22, [lp = OnePole (2500)] (double t, Rng& r) mutable {
        return lp.lp (r.bipolar() + (t < 0.0015 ? 3.0 : 0.0)) * env (t, 25);
    }));
    v.push_back (render ("Tape Hit", 200, 23, [lp = OnePole (3500)] (double t, Rng& r) mutable {
        const double body = std::tanh (3.0 * std::sin (kTwoPi * (120 + 60 * env (t, 20)) * t)) * env (t, 160);
        return lp.lp (body + 0.3 * r.bipolar() * env (t, 30));
    }));
    v.push_back (render ("Bell Ping", 500, 24, [] (double t, Rng&) {
        const double m = std::sin (kTwoPi * 1400 * 3.5 * t) * 2.5 * env (t, 300);
        return std::sin (kTwoPi * 1400 * t + m) * env (t, 480);
    }));
    v.push_back (render ("Stick", 40, 25, [hp = OnePole (2000)] (double t, Rng& r) mutable {
        return (hp.hp (r.bipolar()) * 0.5 + std::sin (kTwoPi * 2600 * t)) * env (t, 30);
    }));
    v.push_back (render ("Breath", 260, 26, [lp = OnePole (1800), hp = OnePole (400)] (double t, Rng& r) mutable {
        const double a = std::min (1.0, t / 0.06);
        return hp.hp (lp.lp (r.bipolar())) * a * env (t, 240);
    }));
    return v;
}
} // namespace

const std::vector<OneShot>& transientSet()
{
    static const std::vector<OneShot> s = build();
    return s;
}

int numTransients() { return (int) transientSet().size(); }
} // namespace df
