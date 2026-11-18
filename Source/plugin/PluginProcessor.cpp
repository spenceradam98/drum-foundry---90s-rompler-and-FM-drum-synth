#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "Samples.h"

using namespace juce;

namespace
{
// Per-pad parameter slots, in the order they are cached in `raw`.
enum Slot
{
    sAlg, sOp1Ratio, sOp1Level, sOp1Decay, sOp2Ratio, sOp2Level, sOp2Decay, sOp3Ratio, sOp3Level, sOp3Decay,
    sOp4Ratio, sOp4Level, sOp4Decay, sFeedback, sPitch, sPitchEnv, sPitchEnvTime, sFmLevel,
    sSample, sStart, sSmpLevel, sSmpPitch, sBits, sRate,
    sNoise, sNoiseColour, sNoiseDecay,
    sFilter, sCutoff, sReso, sAttack, sHold, sDecay, sDrive,
    sLevel, sPan, sChoke, sOut, sNote,
    sNumSlots
};

const char* slotId[sNumSlots] = {
    "alg", "op1_ratio", "op1_level", "op1_decay", "op2_ratio", "op2_level", "op2_decay", "op3_ratio", "op3_level", "op3_decay",
    "op4_ratio", "op4_level", "op4_decay", "feedback", "pitch", "pitchenv", "pitchenv_time", "fm_level",
    "sample", "start", "smp_level", "smp_pitch", "bits", "rate",
    "noise", "noise_colour", "noise_decay",
    "filter", "cutoff", "reso", "attack", "hold", "decay", "drive",
    "level", "pan", "choke", "out", "note"
};

NormalisableRange<float> skewed (float lo, float hi, float centre, float step = 0.0f)
{
    NormalisableRange<float> r (lo, hi, step);
    r.setSkewForCentre (centre);
    return r;
}

AudioParameterFloatAttributes fmt (std::function<String (float, int)> f) { return AudioParameterFloatAttributes().withStringFromValueFunction (std::move (f)); }
String pctStr (float v, int) { return String (v, 0) + "%"; }
String msStr (float v, int) { return v < 1000 ? String (v, v < 10 ? 1 : 0) + " ms" : String (v / 1000, 2) + " s"; }
String hzStr (float v, int) { return v < 1000 ? String (v, 0) + " Hz" : String (v / 1000, 2) + " kHz"; }
} // namespace

AudioProcessorValueTreeState::ParameterLayout DrumFoundryProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto fl = [&] (const String& id, const String& name, NormalisableRange<float> r, float def, std::function<String (float, int)> f) {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, fmt (std::move (f))));
    };

    StringArray algs;
    for (int a = 0; a < df::kNumAlgorithms; ++a) algs.add (String (a + 1) + ": " + df::algorithm (a).name);
    StringArray samples;
    for (auto& s : df::transientSet()) samples.add (s.name);
    StringArray outs { "Main" };
    for (int i = 1; i <= 8; ++i) outs.add ("Out " + String (i));

    const auto& kit = df::factoryKits().front();
    for (int pad = 0; pad < df::kNumPads; ++pad)
    {
        const auto& v = kit.pads[(size_t) pad];
        const String n = String (df::padName (pad)) + " ";
        auto id = [pad] (int slot) { return pid (pad, slotId[slot]); };

        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id (sAlg), 1 }, n + "Algorithm", algs, v.algorithm));
        for (int o = 0; o < df::kNumOps; ++o)
        {
            const String on = n + "Op " + String (o + 1) + " ";
            const auto& op = v.op[(size_t) o];
            fl (id (sOp1Ratio + o * 3), on + "Ratio", skewed (0.25f, 16.0f, 2.0f, 0.001f), op.ratio, [] (float x, int) { return String (x, 3) + "x"; });
            fl (id (sOp1Level + o * 3), on + "Level", { 0, 100, 0.1f }, op.level * 100, pctStr);
            fl (id (sOp1Decay + o * 3), on + "Decay", skewed (1, 4000, 250), op.decayMs, msStr);
        }
        fl (id (sFeedback), n + "Feedback", { 0, 100, 0.1f }, v.feedback * 100, pctStr);
        fl (id (sPitch), n + "Pitch", skewed (20, 5000, 300), v.pitchHz, hzStr);
        fl (id (sPitchEnv), n + "Pitch Env", { 0, 48, 0.01f }, v.pitchEnvSemis, [] (float x, int) { return String (x, 1) + " st"; });
        fl (id (sPitchEnvTime), n + "Pitch Env Time", skewed (1, 500, 40), v.pitchEnvMs, msStr);
        fl (id (sFmLevel), n + "FM Level", { 0, 100, 0.1f }, v.fmLevel * 100, pctStr);

        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id (sSample), 1 }, n + "Sample", samples, v.sample));
        fl (id (sStart), n + "Sample Start", { 0, 100, 0.1f }, v.sampleStart * 100, pctStr);
        fl (id (sSmpLevel), n + "Sample Level", { 0, 100, 0.1f }, v.sampleLevel * 100, pctStr);
        fl (id (sSmpPitch), n + "Sample Pitch", { -24, 24, 0.01f }, v.samplePitch, [] (float x, int) { return (x > 0 ? "+" : "") + String (x, 1) + " st"; });
        fl (id (sBits), n + "Bit Depth", { 4, 16, 1 }, v.sampleBits, [] (float x, int) { return String ((int) x) + "-bit"; });
        fl (id (sRate), n + "Sample Rate", skewed (8, 44.1f, 22, 0.01f), v.sampleRateKHz, [] (float x, int) { return String (x, 1) + " kHz"; });

        fl (id (sNoise), n + "Noise Level", { 0, 100, 0.1f }, v.noiseLevel * 100, pctStr);
        fl (id (sNoiseColour), n + "Noise Colour", { -100, 100, 0.1f }, v.noiseColour * 100, [] (float x, int) { return std::abs (x) < 1 ? String ("White") : (x < 0 ? "Dark " : "Bright ") + String (std::abs (x), 0); });
        fl (id (sNoiseDecay), n + "Noise Decay", skewed (5, 4000, 250), v.noiseDecayMs, msStr);

        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id (sFilter), 1 }, n + "Filter", StringArray { "Low-pass", "Band-pass", "High-pass" }, (int) v.filter));
        fl (id (sCutoff), n + "Cutoff", skewed (20, 20000, 1500), v.cutoffHz, hzStr);
        fl (id (sReso), n + "Resonance", { 0, 100, 0.1f }, v.resonance * 100, pctStr);
        fl (id (sAttack), n + "Attack", skewed (0, 200, 20), v.attackMs, msStr);
        fl (id (sHold), n + "Hold", skewed (0, 1000, 60), v.holdMs, msStr);
        fl (id (sDecay), n + "Decay", skewed (5, 6000, 400), v.decayMs, msStr);
        fl (id (sDrive), n + "Drive", { 0, 100, 0.1f }, v.drive * 100, pctStr);

        fl (id (sLevel), n + "Level", skewed (-60, 6, -12, 0.1f), v.levelDb, [] (float x, int) { return x <= -59.9f ? String ("-inf") : String (x, 1) + " dB"; });
        fl (id (sPan), n + "Pan", { -100, 100, 1 }, v.pan * 100, [] (float x, int) { return std::abs (x) < 0.5f ? String ("C") : (x < 0 ? "L" : "R") + String (std::abs (x), 0); });
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id (sChoke), 1 }, n + "Choke Group", StringArray { "Off", "1", "2", "3", "4" }, v.chokeGroup));
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id (sOut), 1 }, n + "Output", outs, v.outBus));
        p.push_back (std::make_unique<AudioParameterInt> (ParameterID { id (sNote), 1 }, n + "MIDI Note", 0, 127, v.note));
    }

    fl ("crush", "Master Crush", { 0, 100, 0.1f }, 0, pctStr);
    fl ("output", "Output", { -24, 12, 0.1f }, 0, [] (float x, int) { return String (x, 1) + " dB"; });
    return { p.begin(), p.end() };
}

DrumFoundryProcessor::DrumFoundryProcessor()
    : AudioProcessor ([] {
          BusesProperties b;
          b = b.withOutput ("Main", AudioChannelSet::stereo(), true);
          for (int i = 1; i <= 8; ++i) b = b.withOutput ("Out " + String (i), AudioChannelSet::stereo(), false);
          return b;
      }()),
      apvts (*this, nullptr, "DrumFoundry", createLayout())
{
    for (int pad = 0; pad < df::kNumPads; ++pad)
        for (int s = 0; s < sNumSlots; ++s)
            raw[(size_t) pad][(size_t) s] = apvts.getRawParameterValue (pid (pad, slotId[s]));
    crushParam = apvts.getRawParameterValue ("crush");
    outputParam = apvts.getRawParameterValue ("output");
}

bool DrumFoundryProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::stereo()) return false;
    for (int i = 1; i < layouts.outputBuses.size(); ++i)
    {
        const auto& set = layouts.outputBuses.getReference (i);
        if (! set.isDisabled() && set != AudioChannelSet::stereo()) return false;
    }
    return layouts.inputBuses.isEmpty();
}

void DrumFoundryProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, jmax (32, samplesPerBlock));
}

df::VoiceParams DrumFoundryProcessor::readPad (int pad) const
{
    const auto& r = raw[(size_t) pad];
    auto v = [&] (int s) { return r[(size_t) s]->load(); };
    df::VoiceParams p;
    p.algorithm = (int) v (sAlg);
    for (int o = 0; o < df::kNumOps; ++o)
        p.op[(size_t) o] = { v (sOp1Ratio + o * 3), v (sOp1Level + o * 3) / 100.0f, v (sOp1Decay + o * 3) };
    p.feedback = v (sFeedback) / 100.0f;
    p.pitchHz = v (sPitch);
    p.pitchEnvSemis = v (sPitchEnv);
    p.pitchEnvMs = v (sPitchEnvTime);
    p.fmLevel = v (sFmLevel) / 100.0f;
    p.sample = (int) v (sSample);
    p.sampleStart = v (sStart) / 100.0f;
    p.sampleLevel = v (sSmpLevel) / 100.0f;
    p.samplePitch = v (sSmpPitch);
    p.sampleBits = v (sBits);
    p.sampleRateKHz = v (sRate);
    p.noiseLevel = v (sNoise) / 100.0f;
    p.noiseColour = v (sNoiseColour) / 100.0f;
    p.noiseDecayMs = v (sNoiseDecay);
    p.filter = (df::FilterType) (int) v (sFilter);
    p.cutoffHz = v (sCutoff);
    p.resonance = v (sReso) / 100.0f;
    p.attackMs = v (sAttack);
    p.holdMs = v (sHold);
    p.decayMs = v (sDecay);
    p.drive = v (sDrive) / 100.0f;
    p.levelDb = v (sLevel);
    p.pan = v (sPan) / 100.0f;
    p.chokeGroup = (int) v (sChoke);
    p.outBus = (int) v (sOut);
    p.note = (int) v (sNote);
    return p;
}

void DrumFoundryProcessor::writePad (int pad, const df::VoiceParams& p, bool keepRouting)
{
    auto set = [&] (int slot, float value) {
        if (auto* param = apvts.getParameter (pid (pad, slotId[slot])))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };
    set (sAlg, (float) p.algorithm);
    for (int o = 0; o < df::kNumOps; ++o)
    {
        set (sOp1Ratio + o * 3, p.op[(size_t) o].ratio);
        set (sOp1Level + o * 3, p.op[(size_t) o].level * 100);
        set (sOp1Decay + o * 3, p.op[(size_t) o].decayMs);
    }
    set (sFeedback, p.feedback * 100);
    set (sPitch, p.pitchHz);
    set (sPitchEnv, p.pitchEnvSemis);
    set (sPitchEnvTime, p.pitchEnvMs);
    set (sFmLevel, p.fmLevel * 100);
    set (sSample, (float) p.sample);
    set (sStart, p.sampleStart * 100);
    set (sSmpLevel, p.sampleLevel * 100);
    set (sSmpPitch, p.samplePitch);
    set (sBits, p.sampleBits);
    set (sRate, p.sampleRateKHz);
    set (sNoise, p.noiseLevel * 100);
    set (sNoiseColour, p.noiseColour * 100);
    set (sNoiseDecay, p.noiseDecayMs);
    set (sFilter, (float) (int) p.filter);
    set (sCutoff, p.cutoffHz);
    set (sReso, p.resonance * 100);
    set (sAttack, p.attackMs);
    set (sHold, p.holdMs);
    set (sDecay, p.decayMs);
    set (sDrive, p.drive * 100);
    set (sLevel, p.levelDb);
    set (sPan, p.pan * 100);
    if (! keepRouting)
    {
        set (sChoke, (float) p.chokeGroup);
        set (sOut, (float) p.outBus);
        set (sNote, (float) p.note);
    }
}

void DrumFoundryProcessor::setCurrentProgram (int index)
{
    const auto& kits = df::factoryKits();
    currentKit = jlimit (0, (int) kits.size() - 1, index);
    const auto& kit = kits[(size_t) currentKit];
    for (int pad = 0; pad < df::kNumPads; ++pad)
    {
        // a kit sets the sound and the choke group; the MIDI note and output routing stay as the user set them
        writePad (pad, kit.pads[(size_t) pad], true);
        if (auto* param = apvts.getParameter (pid (pad, slotId[sChoke])))
            param->setValueNotifyingHost (param->convertTo0to1 ((float) kit.pads[(size_t) pad].chokeGroup));
    }
}

const String DrumFoundryProcessor::getProgramName (int index)
{
    const auto& kits = df::factoryKits();
    return isPositiveAndBelow (index, (int) kits.size()) ? String (kits[(size_t) index].name) : String();
}

void DrumFoundryProcessor::loadVoicePreset (int preset, int pad)
{
    const auto& vp = df::voicePresets();
    if (isPositiveAndBelow (preset, (int) vp.size())) writePad (pad, vp[(size_t) preset].p, true);
}

void DrumFoundryProcessor::resetPadToKit (int pad)
{
    writePad (pad, df::factoryKits()[(size_t) currentKit].pads[(size_t) pad], true);
}

bool DrumFoundryProcessor::isBusEnabledForPad (int pad) const
{
    const int b = (int) raw[(size_t) pad][sOut]->load();
    if (b == 0) return true;
    auto* bus = getBus (false, b);
    return bus != nullptr && bus->isEnabled();
}

void DrumFoundryProcessor::renderRange (AudioBuffer<float>& buffer, int start, int n)
{
    while (n > 0)
    {
        const int chunk = jmin (n, 512);
        engine.process (chunk);
        for (int b = 0; b < getBusCount (false) && b < df::kNumOutBuses; ++b)
        {
            auto* bus = getBus (false, b);
            if (bus == nullptr || ! bus->isEnabled()) continue;
            auto out = getBusBuffer (buffer, false, b);
            for (int ch = 0; ch < jmin (2, out.getNumChannels()); ++ch)
                FloatVectorOperations::copy (out.getWritePointer (ch, start), engine.bus (b, ch), chunk);
        }
        start += chunk;
        n -= chunk;
    }
}

void DrumFoundryProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    buffer.clear();

    for (int pad = 0; pad < df::kNumPads; ++pad) engine.setPad (pad, readPad (pad));
    engine.setGlobal ({ crushParam->load() / 100.0f, outputParam->load() });
    for (int b = 0; b < df::kNumOutBuses; ++b)
    {
        auto* bus = getBus (false, b);
        engine.setBusActive (b, bus != nullptr && bus->isEnabled());
    }

    if (const auto a = pendingAudition.exchange (0))
        for (int pad = 0; pad < df::kNumPads; ++pad)
            if (a & (1u << pad)) engine.noteOn (pad, 0.85f);

    // sample-accurate MIDI: render up to each event, then apply it
    int pos = 0;
    const int total = buffer.getNumSamples();
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int t = jlimit (0, total, meta.samplePosition);
        if (t > pos) { renderRange (buffer, pos, t - pos); pos = t; }
        if (msg.isNoteOn()) engine.midiNote (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) engine.allNotesOff();
    }
    if (pos < total) renderRange (buffer, pos, total - pos);
    voicesInUse.store (engine.activeVoices());
}

AudioProcessorEditor* DrumFoundryProcessor::createEditor() { return new DrumFoundryEditor (*this); }

void DrumFoundryProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("theme", themeName, nullptr);
    state.setProperty ("kit", currentKit, nullptr);
    state.setProperty ("selectedPad", selectedPad, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void DrumFoundryProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (! xml->hasTagName (apvts.state.getType())) return;
        auto tree = ValueTree::fromXml (*xml);
        themeName = tree.getProperty ("theme", themeName).toString();
        currentKit = jlimit (0, getNumPrograms() - 1, (int) tree.getProperty ("kit", 0));
        selectedPad = jlimit (0, df::kNumPads - 1, (int) tree.getProperty ("selectedPad", 0));
        apvts.replaceState (tree);
    }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DrumFoundryProcessor(); }
