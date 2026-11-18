# Drum Foundry: 90s rompler / 4-op FM percussion synth (design, v0.1)

Stack: JUCE 8.0.4, C++17, CMake, VST3 + AU + Standalone. It uses the same layout as Driftglass, Comma and Crate12:
a JUCE-free engine library with its own tests and a thin plugin wrapper. The DSP is scalar only, so it runs on
macOS 11 Intel machines without AVX2. Family: vintage groovebox & sampler, pairs with crate12, outrigger and platterbox.

## Signal flow (per voice)

```
 note -> pad -> voice (16-voice pool, choke groups)
           FM: 4 sine ops, 8 algorithms, op-4 feedback, pitch env  --(2x oversampled, 4-pole decimator)--+
           Transient: built-in one-shot -> zero-order-hold @ 8..44.1 kHz -> 4..16-bit quantiser ---------+-> drive -> SVF LP/BP/HP -> amp AHD -> level/pan -> bus
           Noise: white -> one-pole tilt (dark .. bright) -> AD envelope ---------------------------------+
 buses: Main + Out 1..8 (stereo)  ->  master crush (shared ZOH + bit path)  ->  output gain
```

- **FM:** 4 operators with ratio (0.25 to 16x), level and decay each. On a carrier, level sets the amplitude. On a
  modulator it sets the modulation index (up to about 7 rad, scaled by velocity). Op 4 has self-feedback. The pitch
  envelope sweeps down by 0 to 48 semitones over its time, for kicks and toms. The FM runs at 2x the host rate and is
  decimated through a 4-pole Butterworth, so high-ratio metallic patches stay clean.
- **Algorithms:** 1 Stack 4>3>2>1, 2 Branch (3+4)>2>1, 3 Y 4>3>1 + 2>1, 4 Two pairs, 5 Fan 4>(1,2,3),
  6 Stack 4>3>2 + 1, 7 One pair 4>3 + 1 + 2, 8 Additive. The UI draws the selected algorithm.
- **Transient layer:** 16 one-shots, stored at 44.1 kHz / 16-bit: Thump, Click, Snap, Rim, Noise Burst, Clap Multi,
  Shaker, Metal Tick, Wood, Zap, Crash Wash, Vinyl Pop, Tape Hit, Bell Ping, Stick and Breath. Playback uses
  crate12's converter idea, a zero-order-hold "DAC" plus an n-bit quantiser, with the bit depth (4 to 16) and the hold
  rate (8 to 44.1 kHz) made variable. There is no anti-alias filter in front of the hold, on purpose: the aliasing is
  part of the 90s ROM sound.
- **Original content:** every transient is rendered at load time from synthesis and seeded noise (`Samples.cpp`). No
  recordings or commercial ROM samples are included, and the set is identical on every machine.
- **Shape:** tanh drive (normalised), then a TPT state-variable filter (LP/BP/HP with resonance), then an
  attack/hold/decay amp envelope. Velocity scales level (curve 1.3), decay (70 to 100%) and FM index (50 to 100%).
- **Voices:** 16-voice pool. If all are busy, the oldest is stolen. A new hit on a pad fades the pad's previous voice
  over 5 ms, and a hit on any pad in the same choke group fades those voices too (closed hat cuts open hat by default).
- **Multi-out:** the plugin exposes Main plus 8 stereo aux outputs, disabled by default. Each pad picks Main or
  Out 1 to 8. If the host has not enabled that output, the pad plays on Main instead, and the pad shows "(off: Main)".
- **Master crush:** one knob lowers every bus from 16 to 6 bits and from 44.1 kHz down to about 12 kHz.

## Presets

16 factory kits, exposed as host programs: 909-ish House, Rave Metal, Jungle Crunch, Electro Zap, Trip-Hop Dust,
Industrial FM, Big Beat, Lo-Fi Toy, Deep Garage, Acid Tribal, Hardcore Gabba, Minimal Click, Boom Bap Chrome,
Dub Cavern, Breakbeat Steel and Game Console. Some kits set one crunch for every pad (for example Lo-Fi Toy at
6-bit / 8 kHz and Jungle Crunch at 8-bit / 16 kHz). Loading a kit changes the sounds and choke groups, but keeps your
MIDI notes and output routing.

24 voice presets (kicks, snares, claps, hats, toms, cymbals, perc) can be loaded onto any pad from the pad's
right-click menu or the **Voice presets...** button. To copy a sound between pads, drag one pad onto another
or use "Copy sound to". Neither changes the target pad's note or output.

## UI

The pad grid is 4x2 on the left: click a pad to audition and select it, and pads light up on MIDI. Below the grid
are the pad's level, pan, MIDI note, choke group and output. On the right are the FM section (algorithm diagram,
4 operator columns, pitch and pitch envelope), the transient/crunch and noise sections, and the shape section with
an envelope display. An LCD shows the kit, the selected pad, its bit depth and rate, and the voice count.
There are four themes, saved with the session: Rack Grey, Hot Pink 90s, Chrome and Studio Black.

## Decisions made for v0.1

- **No built-in pattern sequencer.** Sequence it from the DAW, outrigger or kaleido-arp. Swing is left out with it.
- **Note-off is ignored.** Drums are one-shots, and the amp envelope sets the length.
- **One parameter set per pad,** all automatable (about 39 per pad, 314 in total). Host programs are the kits.
- **Pad notes are parameters,** so a remapped kit is saved and recalled with the session.

## Testing

`tests/EngineTests.cpp` (32 checks) covers:
- the transient set: count, normalisation, unique names, determinism;
- silence with no input;
- all 128 kit pads and all voice presets: they sound, stay bounded and finish;
- all 8 algorithms at maximum index and feedback;
- algorithm structure, carrier pitch accuracy, the quantiser and hold-clock rate;
- choke, the 16-voice cap, multi-out routing and fallback;
- velocity, the GM map and aliases, sample-rate independence, and master crush.

Not tested yet: multi-out in real hosts. Logic AU multi-out needs the host's multi-output instance, and Ableton,
REAPER and FL Studio each handle aux buses differently. Check all four before a release.
