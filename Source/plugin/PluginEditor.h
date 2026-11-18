#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "Themes.h"

class DrumFoundryEditor;

// Rotary knob with its value under it and a name label at the bottom.
struct Knob : juce::Component
{
    Knob (const juce::String& name, const juce::String& paramSuffix, bool bipolar = false);
    void resized() override;
    juce::Slider slider;
    juce::Label label;
    juce::String suffix;   // parameter id without the "pN_" prefix ("" for globals)
};

struct PadButton : juce::Component, juce::DragAndDropTarget
{
    PadButton (DrumFoundryEditor& e, int index) : editor (e), pad (index) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHover = false; repaint(); }
    void itemDropped (const SourceDetails&) override;

    DrumFoundryEditor& editor;
    int pad;
    float lit = 0.0f;
    bool dropHover = false;
};

struct AlgorithmView : juce::Component
{
    void paint (juce::Graphics&) override;
    int algorithm = 0;
    float feedback = 0.0f;
    const Theme* theme = nullptr;
};

struct EnvelopeView : juce::Component
{
    void paint (juce::Graphics&) override;
    df::VoiceParams p;
    const Theme* theme = nullptr;
};

class DrumFoundryEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit DrumFoundryEditor (DrumFoundryProcessor&);
    ~DrumFoundryEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void selectPad (int pad);
    void showPadMenu (int pad);
    DrumFoundryProcessor& proc;
    int selected = 0;

    void applyTheme (const juce::String& name);   // public so the screenshot tool can use it

private:
    void timerCallback() override;
    void attachPad();
    void refreshViews();
    void refreshKitBox();
    juce::Rectangle<int> panel (juce::Rectangle<int> r, const juce::String& title);

    FoundryLookAndFeel lnf;
    std::vector<std::unique_ptr<PadButton>> pads;
    std::vector<std::unique_ptr<Knob>> knobs;     // per-pad knobs
    juce::ComboBox algoBox, sampleBox, filterBox, chokeBox, outBox, kitBox, themeBox;
    juce::TextButton prevKit { "<" }, nextKit { ">" }, presetButton { "Voice presets..." };
    Knob crushKnob { "Crush", "" }, outputKnob { "Output", "" };
    AlgorithmView algoView;
    EnvelopeView envView;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
    juce::AudioProcessorValueTreeState::SliderAttachment crushAtt, outputAtt;

    // panel rectangles, filled in resized() and drawn in paint()
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> panels;
    juce::Rectangle<int> lcdArea, titleArea;
    std::array<juce::Rectangle<int>, 4> opHeaders;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumFoundryEditor)
};
