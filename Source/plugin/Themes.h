// UI themes: Rack Grey, Hot Pink 90s, Chrome, Studio Black. Saved per session in the plugin state.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

struct Theme
{
    juce::String name;
    juce::Colour bg, bg2, panel, edge, text, dim, accent, accent2, knob, track, lcdBg, lcdText, pad, padLit;
    int knobStyle = 0;      // 0 = arc ring, 1 = hardware cap with pointer, 2 = flat dot
    bool brushed = false;   // brushed-metal stripes on the background
    bool bevel = false;     // raised 3D bevels on pads and panels
};

const std::vector<Theme>& allThemes();
const Theme& themeByName (const juce::String& name);

class FoundryLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FoundryLookAndFeel();
    void setTheme (const Theme& t);
    const Theme& theme() const { return *th; }

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;

private:
    const Theme* th = nullptr;
};
