#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine.h"
#include "Presets.h"

class DrumFoundryProcessor : public juce::AudioProcessor
{
public:
    DrumFoundryProcessor();
    ~DrumFoundryProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return (int) df::factoryKits().size(); }
    int getCurrentProgram() override { return currentKit; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- parameter helpers
    static juce::String pid (int pad, const juce::String& name) { return "p" + juce::String (pad + 1) + "_" + name; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    df::VoiceParams readPad (int pad) const;
    void writePad (int pad, const df::VoiceParams& v, bool keepRouting);

    // ---- UI hooks (message thread)
    void auditionPad (int pad) { pendingAudition.fetch_or (1u << pad); }
    void copyPad (int from, int to) { writePad (to, readPad (from), true); }
    void loadVoicePreset (int preset, int pad);
    void resetPadToKit (int pad);
    float padLevel (int pad) const { return engine.padLevel (pad); }
    uint32_t padTriggers (int pad) const { return engine.padTriggerCount (pad); }
    int activeVoices() const { return voicesInUse.load(); }
    juce::String kitName() const { return df::factoryKits()[(size_t) currentKit].name; }
    bool isBusEnabledForPad (int pad) const;

    juce::String getThemeName() const { return themeName; }
    void setThemeName (const juce::String& t) { themeName = t; }
    int getSelectedPad() const { return selectedPad; }
    void setSelectedPad (int p) { selectedPad = juce::jlimit (0, df::kNumPads - 1, p); }

    juce::AudioProcessorValueTreeState apvts;

private:
    void renderRange (juce::AudioBuffer<float>& buffer, int start, int n);

    df::Engine engine;
    std::array<std::array<std::atomic<float>*, 64>, df::kNumPads> raw {};   // cached parameter pointers
    std::atomic<float>* crushParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<uint32_t> pendingAudition { 0 };
    std::atomic<int> voicesInUse { 0 };
    juce::String themeName { "Rack Grey" };
    int selectedPad = 0;
    int currentKit = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumFoundryProcessor)
};
