// Engine tests for drum-foundry. Plain C++, no framework: each check prints and counts failures.
#include <cmath>
#include <cstdio>
#include <set>
#include <string>

#include "Engine.h"
#include "Presets.h"
#include "Samples.h"

using namespace df;

static int failures = 0;
#define CHECK(cond, msg)                                                         \
    do {                                                                         \
        if (! (cond)) { std::printf ("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); ++failures; } \
        else std::printf ("ok:   %s\n", msg);                                    \
    } while (0)

struct Stats { float peak = 0; double energy = 0; bool finite = true; };

static Stats render (Engine& e, int samples, int bus = 0, int block = 256)
{
    Stats s;
    for (int done = 0; done < samples; done += block)
    {
        const int n = std::min (block, samples - done);
        e.process (n);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
            {
                const float x = e.bus (bus, ch)[i];
                if (! std::isfinite (x)) s.finite = false;
                s.peak = std::max (s.peak, std::abs (x));
                s.energy += (double) x * x;
            }
    }
    return s;
}

static void loadKit (Engine& e, const Kit& k) { for (int i = 0; i < kNumPads; ++i) e.setPad (i, k.pads[(size_t) i]); }

int main()
{
    // ---- built-in transient set
    {
        const auto& set = transientSet();
        CHECK (set.size() == 16, "16 built-in transients");
        bool ok = true;
        std::set<std::string> names;
        for (auto& s : set)
        {
            names.insert (s.name);
            int peak = 0;
            for (auto x : s.data) peak = std::max (peak, std::abs ((int) x));
            ok = ok && s.data.size() > 400 && peak > 20000 && peak <= 32767;
        }
        CHECK (ok, "every transient is non-empty and normalised");
        CHECK (names.size() == set.size(), "transient names are unique");
        CHECK (transientSet()[5].data == set[5].data, "transient set is deterministic");
    }

    // ---- silence without notes
    {
        Engine e;
        e.prepare (48000, 256);
        loadKit (e, factoryKits()[0]);
        const auto s = render (e, 48000);
        CHECK (s.peak == 0.0f, "silence with no notes");
    }

    // ---- every pad of every kit makes finite, bounded, audible sound and then stops
    {
        bool allOk = true;
        for (auto& kit : factoryKits())
            for (int pad = 0; pad < kNumPads; ++pad)
            {
                Engine e;
                e.prepare (48000, 256);
                loadKit (e, kit);
                e.noteOn (pad, 1.0f);
                const auto s = render (e, 48000 * 4);
                const bool ok = s.finite && s.peak > 0.01f && s.peak < 4.0f && e.activeVoices() == 0;
                if (! ok) std::printf ("  kit %s pad %d: peak %f finite %d voices %d\n", kit.name.c_str(), pad, s.peak, (int) s.finite, e.activeVoices());
                allOk = allOk && ok;
            }
        CHECK (factoryKits().size() == 16, "16 factory kits");
        CHECK (allOk, "all 128 kit pads sound, stay bounded and end");
    }

    // ---- voice presets
    {
        bool ok = true;
        for (auto& vp : voicePresets())
        {
            Engine e;
            e.prepare (44100, 256);
            e.setPad (0, vp.p);
            e.noteOn (0, 0.9f);
            const auto s = render (e, 44100 * 3);
            ok = ok && s.finite && s.peak > 0.01f && s.peak < 4.0f;
        }
        CHECK (voicePresets().size() >= 20, "voice preset library has 20+ entries");
        CHECK (ok, "all voice presets sound and stay bounded");
    }

    // ---- every FM algorithm is stable at full index and feedback
    {
        bool ok = true;
        for (int a = 0; a < kNumAlgorithms; ++a)
        {
            VoiceParams v;
            v.algorithm = a;
            for (auto& op : v.op) op = { 3.7f, 1.0f, 500.0f };
            v.feedback = 1.0f;
            v.pitchHz = 2000;
            v.decayMs = 500;
            Engine e;
            e.prepare (44100, 128);
            e.setPad (0, v);
            e.noteOn (0, 1.0f);
            const auto s = render (e, 44100);
            ok = ok && s.finite && s.peak > 0.01f && s.peak < 3.0f;
        }
        CHECK (ok, "all 8 algorithms stable at max index + feedback");
    }

    // ---- carriers: an additive patch is louder than a pure stack modulator
    {
        const auto& stack = algorithm (0);
        int c = 0; for (bool b : stack.carrier) c += b;
        CHECK (c == 1, "stack algorithm has one carrier");
        const auto& add = algorithm (7);
        c = 0; for (bool b : add.carrier) c += b;
        CHECK (c == 4, "additive algorithm has four carriers");
        bool sourcesHigher = true;
        for (int a = 0; a < kNumAlgorithms; ++a)
            for (int t = 0; t < kNumOps; ++t)
                for (int s = 0; s <= t; ++s)
                    if (algorithm (a).mod[(size_t) t][(size_t) s]) sourcesHigher = false;
        CHECK (sourcesHigher, "modulators always come from higher-numbered ops");
    }

    // ---- pitch: a plain sine carrier at 1 kHz crosses zero ~2000 times a second
    {
        VoiceParams v;
        v.op[0] = { 1.0f, 1.0f, 100000.0f };
        v.pitchHz = 1000;
        v.cutoffHz = 20000;
        v.holdMs = 2000; v.decayMs = 5000;
        Engine e;
        e.prepare (48000, 480);
        e.setPad (0, v);
        e.noteOn (0, 1.0f);
        e.process (480);   // skip the start
        int crossings = 0; float prev = 0;
        for (int b = 0; b < 100; ++b)
        {
            e.process (480);
            for (int i = 0; i < 480; ++i) { const float x = e.bus (0, 0)[i]; if ((x >= 0) != (prev >= 0)) ++crossings; prev = x; }
        }
        CHECK (std::abs (crossings - 2000) < 10, "1 kHz carrier has the right pitch");
    }

    // ---- bit crush: 4-bit transient at full level has only a handful of distinct values
    {
        VoiceParams v;
        v.fmLevel = 0; v.sample = 4; v.sampleLevel = 1.0f; v.sampleBits = 4; v.sampleRateKHz = 44.1f;
        v.cutoffHz = 20000; v.holdMs = 1000; v.decayMs = 5000;   // filter ~open, amp flat during the hold
        v.filter = FilterType::LowPass;
        Engine e;
        e.prepare (44100, 4096);
        e.setPad (0, v);
        // bypass the SVF influence by checking the raw crusher instead: quantise directly
        std::set<float> levels;
        for (int i = 0; i < 4000; ++i) levels.insert (Crusher::quantise (std::sin (i * 0.01f), 4));
        CHECK (levels.size() <= 17, "4-bit quantiser has at most 17 levels");
        CHECK (Crusher::quantise (0.3333f, 16) == 0.3333f, "16-bit setting is transparent");
        // and the hold clock: at 8 kHz from 44.1 kHz, values change at most every ~5.5 samples
        Crusher c; int changes = 0; float last = -2;
        for (int i = 0; i < 44100; ++i)
        {
            const float y = c.tick (8000, 44100, 16, [i] { return std::sin (i * 0.05f); });
            if (y != last) { ++changes; last = y; }
        }
        CHECK (changes <= 8001 && changes >= 7900, "8 kHz hold rate ticks 8000 times a second");
    }

    // ---- choke: the closed hat cuts the open hat
    {
        Engine e;
        e.prepare (48000, 256);
        loadKit (e, factoryKits()[0]);
        e.noteOn (4, 1.0f);       // open hat
        render (e, 2400);
        CHECK (e.activeVoices() == 1, "open hat is ringing");
        e.noteOn (3, 1.0f);       // closed hat
        render (e, 48000 / 20);   // 50 ms: the choke fade is 5 ms
        // after the fade only the closed hat (or nothing) remains
        CHECK (e.activeVoices() <= 1, "open hat is choked by the closed hat");
    }

    // ---- polyphony is capped at 16 and stealing keeps working
    {
        Engine e;
        e.prepare (44100, 256);
        VoiceParams v;
        v.op[0] = { 1.0f, 1.0f, 5000 }; v.holdMs = 5000; v.decayMs = 5000;
        for (int p = 0; p < kNumPads; ++p) e.setPad (p, v);
        for (int i = 0; i < 40; ++i) e.noteOn (i % kNumPads, 1.0f);
        render (e, 256);
        CHECK (e.activeVoices() <= kMaxVoices, "never more than 16 voices");
    }

    // ---- multi-out routing and fallback
    {
        Engine e;
        e.prepare (44100, 256);
        auto kit = factoryKits()[0];
        kit.pads[0].outBus = 3;
        loadKit (e, kit);
        e.setBusActive (3, true);
        e.noteOn (0, 1.0f);
        Stats onBus3, onMain;
        for (int b = 0; b < 40; ++b)
        {
            e.process (256);
            for (int i = 0; i < 256; ++i)
            {
                onBus3.peak = std::max (onBus3.peak, std::abs (e.bus (3, 0)[i]));
                onMain.peak = std::max (onMain.peak, std::abs (e.bus (0, 0)[i]));
            }
        }
        CHECK (onBus3.peak > 0.01f && onMain.peak == 0.0f, "kick routed to out 3 appears only on out 3");

        Engine f;
        f.prepare (44100, 256);
        loadKit (f, kit);           // bus 3 left inactive
        f.noteOn (0, 1.0f);
        const auto s = render (f, 44100 / 4, 0);
        CHECK (s.peak > 0.01f, "inactive bus falls back to main");
    }

    // ---- velocity scales level
    {
        auto peakAt = [] (float vel) {
            Engine e;
            e.prepare (44100, 256);
            loadKit (e, factoryKits()[0]);
            e.noteOn (1, vel);
            return render (e, 44100 / 2).peak;
        };
        const float loud = peakAt (1.0f), soft = peakAt (0.3f);
        CHECK (soft < loud * 0.5f && soft > 0.0f, "velocity scales level");
    }

    // ---- GM note map and aliases
    {
        std::array<int, kNumPads> notes = kDefaultNotes;
        CHECK (padForNote (36, notes) == 0 && padForNote (38, notes) == 1 && padForNote (39, notes) == 2, "GM kick/snare/clap");
        CHECK (padForNote (42, notes) == 3 && padForNote (46, notes) == 4 && padForNote (45, notes) == 5, "GM hats/tom");
        CHECK (padForNote (49, notes) == 6 && padForNote (37, notes) == 7, "GM cymbal/perc");
        CHECK (padForNote (35, notes) == 0 && padForNote (40, notes) == 1 && padForNote (44, notes) == 3, "GM aliases 35/40/44");
        CHECK (padForNote (41, notes) == 5 && padForNote (51, notes) == 6, "GM aliases toms/ride");
        CHECK (padForNote (60, notes) == -1, "unmapped note ignored");
        notes[0] = 60;
        CHECK (padForNote (60, notes) == 0, "custom pad note wins");
    }

    // ---- sample-rate independence: decay length in seconds stays the same
    {
        auto tail = [] (double fs) {
            Engine e;
            e.prepare (fs, 256);
            loadKit (e, factoryKits()[0]);
            e.noteOn (0, 1.0f);
            int blocks = 0;
            while (e.activeVoices() > 0 && blocks < 10000) { e.process (256); ++blocks; }
            return blocks * 256 / fs;
        };
        const double a = tail (44100), b = tail (96000);
        CHECK (std::abs (a - b) < 0.05, "kick length is the same at 44.1 and 96 kHz");
    }

    // ---- master crush changes the sound but stays bounded
    {
        Engine e;
        e.prepare (44100, 256);
        loadKit (e, factoryKits()[0]);
        GlobalParams g; g.masterCrush = 1.0f;
        e.setGlobal (g);
        e.noteOn (1, 1.0f);
        const auto s = render (e, 44100);
        CHECK (s.finite && s.peak > 0.01f && s.peak < 4.0f, "master crush at max is bounded");
    }

    std::printf ("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
