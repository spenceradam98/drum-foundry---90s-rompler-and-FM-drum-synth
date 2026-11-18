#include "Themes.h"

using namespace juce;

const std::vector<Theme>& allThemes()
{
    static const std::vector<Theme> t = [] {
        std::vector<Theme> v;
        auto add = [&] (const char* name, uint32 bg, uint32 bg2, uint32 panel, uint32 edge, uint32 text, uint32 dim,
                        uint32 accent, uint32 accent2, uint32 knob, uint32 track, uint32 lcdBg, uint32 lcdText,
                        uint32 pad, uint32 padLit, int style, bool brushed, bool bevel)
        {
            v.push_back ({ name, Colour (bg), Colour (bg2), Colour (panel), Colour (edge), Colour (text), Colour (dim),
                           Colour (accent), Colour (accent2), Colour (knob), Colour (track), Colour (lcdBg), Colour (lcdText),
                           Colour (pad), Colour (padLit), style, brushed, bevel });
        };
        //   name             bg          bg2         panel       edge        text        dim         accent      accent2     knob        track       lcdBg       lcdText     pad         padLit
        add ("Rack Grey",     0xff2b2d30, 0xff35383c, 0xff3d4045, 0xff5a5e64, 0xffeceff1, 0xff9ea4ab, 0xffff8a1f, 0xff58c4ff, 0xff4b4f55, 0xff26282b, 0xff1d2a1a, 0xff9cff6e, 0xff4a4e54, 0xffff8a1f, 1, false, true);
        add ("Hot Pink 90s",  0xff1a0b2e, 0xff2a1147, 0xff311552, 0xff7b2fbf, 0xfffff0fb, 0xffb79ad6, 0xffff2fb4, 0xff29f0e0, 0xff3d1c63, 0xff241038, 0xff0b0420, 0xffff5fd2, 0xff3a1960, 0xffff2fb4, 0, false, false);
        add ("Chrome",        0xffb9bfc6, 0xffcfd4da, 0xffdfe3e8, 0xff8b939c, 0xff1d2228, 0xff59626c, 0xffd6302f, 0xff2a63c9, 0xffe9edf1, 0xffa8b0b8, 0xff23302c, 0xff7cf2c6, 0xffc4cad1, 0xffd6302f, 1, true,  true);
        add ("Studio Black",  0xff0d0e10, 0xff15171a, 0xff1b1d21, 0xff2e3238, 0xffe6e8eb, 0xff7d838c, 0xff38d39f, 0xffffc94a, 0xff24272c, 0xff121316, 0xff050607, 0xff38d39f, 0xff202328, 0xff38d39f, 2, false, false);
        return v;
    }();
    return t;
}

const Theme& themeByName (const String& name)
{
    for (auto& t : allThemes()) if (t.name == name) return t;
    return allThemes().front();
}

FoundryLookAndFeel::FoundryLookAndFeel() { setTheme (allThemes().front()); }

void FoundryLookAndFeel::setTheme (const Theme& t)
{
    th = &t;
    setColour (ComboBox::backgroundColourId, t.bg2);
    setColour (ComboBox::textColourId, t.text);
    setColour (ComboBox::outlineColourId, t.edge);
    setColour (ComboBox::arrowColourId, t.dim);
    setColour (PopupMenu::backgroundColourId, t.bg2);
    setColour (PopupMenu::textColourId, t.text);
    setColour (PopupMenu::headerTextColourId, t.accent);
    setColour (PopupMenu::highlightedBackgroundColourId, t.accent.withAlpha (0.35f));
    setColour (PopupMenu::highlightedTextColourId, t.text);
    setColour (TextButton::textColourOffId, t.text);
    setColour (TextButton::textColourOnId, t.text);
    setColour (TextButton::buttonColourId, t.bg2);
    setColour (TextButton::buttonOnColourId, t.accent.withAlpha (0.5f));
    setColour (Label::textColourId, t.text);
    setColour (TooltipWindow::backgroundColourId, t.bg2);
    setColour (TooltipWindow::textColourId, t.text);
    setColour (BubbleComponent::backgroundColourId, t.bg2);
    setColour (BubbleComponent::outlineColourId, t.edge);
    setColour (Slider::textBoxTextColourId, t.text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (AlertWindow::backgroundColourId, t.bg2);
    setColour (AlertWindow::textColourId, t.text);
}

Font FoundryLookAndFeel::getComboBoxFont (ComboBox&) { return Font (FontOptions (12.5f)); }
Font FoundryLookAndFeel::getPopupMenuFont() { return Font (FontOptions (14.0f)); }
Font FoundryLookAndFeel::getTextButtonFont (TextButton&, int h) { return Font (FontOptions (jmin (13.0f, h * 0.55f), Font::bold)); }

void FoundryLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const auto& t = *th;
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float d = jmin (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();
    const float r = d * 0.5f;
    const float angle = start + pos * (end - start);
    const bool bipolar = s.getProperties()["bipolar"];
    const float from = bipolar ? (start + end) * 0.5f : start;
    const Colour arcCol = s.getProperties().contains ("colour") ? Colour ((uint32) (int64) s.getProperties()["colour"]) : t.accent;

    // track + value arc
    Path track; track.addCentredArc (c.x, c.y, r - 2, r - 2, 0, start, end, true);
    g.setColour (t.track);
    g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    Path val; val.addCentredArc (c.x, c.y, r - 2, r - 2, 0, jmin (from, angle), jmax (from, angle), true);
    g.setColour (arcCol);
    g.strokePath (val, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

    const float kr = r - 6.0f;
    if (t.knobStyle == 1)
    {
        // hardware cap: gradient body, pointer line
        ColourGradient grad (t.knob.brighter (0.35f), c.x, c.y - kr, t.knob.darker (0.5f), c.x, c.y + kr, false);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
        g.setColour (t.edge.darker (0.4f));
        g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.0f);
        g.setColour (t.text);
        const Point<float> p1 = c.getPointOnCircumference (kr * 0.25f, angle), p2 = c.getPointOnCircumference (kr * 0.9f, angle);
        g.drawLine ({ p1, p2 }, 2.0f);
    }
    else if (t.knobStyle == 2)
    {
        g.setColour (t.knob);
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
        g.setColour (arcCol);
        const auto p = c.getPointOnCircumference (kr * 0.65f, angle);
        g.fillEllipse (p.x - 2.2f, p.y - 2.2f, 4.4f, 4.4f);
    }
    else
    {
        g.setColour (t.knob);
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
        g.setColour (arcCol.withAlpha (0.9f));
        const Point<float> p1 = c.getPointOnCircumference (kr * 0.35f, angle), p2 = c.getPointOnCircumference (kr * 0.95f, angle);
        g.drawLine ({ p1, p2 }, 2.2f);
    }
}

void FoundryLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    const auto& t = *th;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    Colour fill = b.getToggleState() ? t.accent.withAlpha (0.45f) : t.bg2;
    if (highlighted) fill = fill.brighter (0.08f);
    if (down) fill = fill.darker (0.15f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (t.edge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void FoundryLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    const auto& t = *th;
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (t.bg2);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.hasKeyboardFocus (true) ? t.accent : t.edge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    Path arrow;
    const float ax = (float) w - 12.0f, ay = h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (t.dim);
    g.fillPath (arrow);
}
