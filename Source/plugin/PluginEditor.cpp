#include "PluginEditor.h"

#include "Samples.h"

using namespace juce;

namespace
{
constexpr int kW = 1120, kH = 612;
constexpr int kKnobW = 62, kKnobH = 74;

String noteName (int n) { return MidiMessage::getMidiNoteName (n, true, true, 3); }

const char* kKnobDefs[][3] = {
    // suffix, label, bipolar
    { "op1_ratio", "Ratio", "" }, { "op1_level", "Level", "" }, { "op1_decay", "Decay", "" },
    { "op2_ratio", "Ratio", "" }, { "op2_level", "Level", "" }, { "op2_decay", "Decay", "" },
    { "op3_ratio", "Ratio", "" }, { "op3_level", "Level", "" }, { "op3_decay", "Decay", "" },
    { "op4_ratio", "Ratio", "" }, { "op4_level", "Level", "" }, { "op4_decay", "Decay", "" },
    { "feedback", "Feedback", "" }, { "pitch", "Pitch", "" }, { "pitchenv", "P.Env", "" },
    { "pitchenv_time", "P.Time", "" }, { "fm_level", "FM Level", "" },
    { "start", "Start", "" }, { "smp_level", "Level", "" }, { "smp_pitch", "Pitch", "b" }, { "bits", "Bits", "" }, { "rate", "Rate", "" },
    { "noise", "Level", "" }, { "noise_colour", "Colour", "b" }, { "noise_decay", "Decay", "" },
    { "cutoff", "Cutoff", "" }, { "reso", "Reso", "" }, { "drive", "Drive", "" },
    { "attack", "Attack", "" }, { "hold", "Hold", "" }, { "decay", "Decay", "" },
    { "level", "Level", "" }, { "pan", "Pan", "b" }, { "note", "Note", "" },
};
} // namespace

// ---------------------------------------------------------------- Knob

Knob::Knob (const String& name, const String& paramSuffix, bool bipolar) : suffix (paramSuffix)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::TextBoxBelow, true, 70, 14);
    slider.getProperties().set ("bipolar", bipolar);
    label.setText (name, dontSendNotification);
    label.setJustificationType (Justification::centred);
    label.setFont (Font (FontOptions (11.0f, Font::bold)));
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromBottom (14));
    slider.setBounds (r);
}

// ---------------------------------------------------------------- Pads

void PadButton::paint (Graphics& g)
{
    const auto& t = static_cast<FoundryLookAndFeel&> (editor.getLookAndFeel()).theme();
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const bool sel = editor.selected == pad;

    if (t.bevel)
    {
        ColourGradient grad (t.pad.brighter (0.25f), r.getX(), r.getY(), t.pad.darker (0.3f), r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
    }
    else g.setColour (t.pad);
    g.fillRoundedRectangle (r, 7.0f);

    if (lit > 0.002f)
    {
        g.setColour (t.padLit.withAlpha (jlimit (0.0f, 0.6f, lit * 0.7f)));
        g.fillRoundedRectangle (r, 7.0f);
    }
    g.setColour (dropHover ? t.accent2 : (sel ? t.accent : t.edge));
    g.drawRoundedRectangle (r, 7.0f, sel || dropHover ? 2.5f : 1.0f);

    auto p = editor.proc.readPad (pad);
    auto inner = r.reduced (8.0f);
    g.setColour (t.text);
    g.setFont (Font (FontOptions (14.0f, Font::bold)));
    g.drawText (df::padName (pad), inner.removeFromTop (18), Justification::topLeft);
    g.setColour (t.dim);
    g.setFont (Font (FontOptions (11.0f)));
    g.drawText (noteName (p.note) + "  (" + String (p.note) + ")", inner.removeFromTop (14), Justification::topLeft);

    auto bottom = inner.removeFromBottom (14);
    String tags;
    if (p.chokeGroup > 0) tags << "C" << p.chokeGroup << "  ";
    tags << (p.outBus == 0 ? String ("Main") : "Out " + String (p.outBus));
    if (! editor.proc.isBusEnabledForPad (pad)) tags << " (off: Main)";
    g.drawText (tags, bottom, Justification::bottomLeft);
    g.setColour (t.dim.withAlpha (0.6f));
    g.setFont (Font (FontOptions (22.0f, Font::bold)));
    g.drawText (String (pad + 1), r.reduced (8.0f, 4.0f), Justification::bottomRight);
}

void PadButton::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { editor.showPadMenu (pad); return; }
    editor.selectPad (pad);
    editor.proc.auditionPad (pad);
}

void PadButton::mouseDrag (const MouseEvent& e)
{
    if (e.getDistanceFromDragStart() > 8 && ! editor.isDragAndDropActive())
        editor.startDragging ("pad:" + String (pad), this);
}

bool PadButton::isInterestedInDragSource (const SourceDetails& d)
{
    return d.description.toString().startsWith ("pad:") && d.description.toString().fromFirstOccurrenceOf ("pad:", false, false).getIntValue() != pad;
}

void PadButton::itemDropped (const SourceDetails& d)
{
    dropHover = false;
    const int from = d.description.toString().fromFirstOccurrenceOf ("pad:", false, false).getIntValue();
    editor.proc.copyPad (from, pad);
    editor.selectPad (pad);
}

// ---------------------------------------------------------------- Algorithm diagram

void AlgorithmView::paint (Graphics& g)
{
    if (theme == nullptr) return;
    const auto& t = *theme;
    const auto& al = df::algorithm (algorithm);
    auto r = getLocalBounds().toFloat();

    g.setColour (t.bg.withAlpha (0.5f));
    g.fillRoundedRectangle (r, 6.0f);

    // depth of each op: carriers are 0, a modulator sits one row above its deepest target
    std::array<int, df::kNumOps> depth {};
    for (int s = 0; s < df::kNumOps; ++s)
    {
        depth[(size_t) s] = 0;
        for (int tgt = 0; tgt < s; ++tgt)
            if (al.mod[(size_t) tgt][(size_t) s]) depth[(size_t) s] = jmax (depth[(size_t) s], depth[(size_t) tgt] + 1);
    }
    int maxDepth = 0;
    for (int d : depth) maxDepth = jmax (maxDepth, d);

    const float boxW = 34, boxH = maxDepth >= 3 ? 20.0f : 24.0f;
    const float outY = r.getBottom() - 14;
    const float lowestY = outY - 18 - boxH;
    const float step = maxDepth > 0 ? jmin (boxH + 22.0f, (lowestY - (r.getY() + 10)) / (float) maxDepth) : 0.0f;
    std::array<Rectangle<float>, df::kNumOps> box;
    for (int d = 0; d <= maxDepth; ++d)
    {
        std::vector<int> row;
        for (int o = 0; o < df::kNumOps; ++o) if (depth[(size_t) o] == d) row.push_back (o);
        const float y = lowestY - d * step;
        for (size_t i = 0; i < row.size(); ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) (i + 1) / (float) (row.size() + 1) - boxW * 0.5f;
            box[(size_t) row[i]] = { x, y, boxW, boxH };
        }
    }

    // modulation lines
    g.setColour (t.dim);
    for (int tgt = 0; tgt < df::kNumOps; ++tgt)
        for (int s = 0; s < df::kNumOps; ++s)
            if (al.mod[(size_t) tgt][(size_t) s])
                g.drawLine ({ box[(size_t) s].getCentreX(), box[(size_t) s].getBottom(), box[(size_t) tgt].getCentreX(), box[(size_t) tgt].getY() }, 1.6f);

    // output bus
    float minX = r.getRight(), maxX = r.getX();
    for (int o = 0; o < df::kNumOps; ++o)
        if (al.carrier[(size_t) o])
        {
            g.drawLine (box[(size_t) o].getCentreX(), box[(size_t) o].getBottom(), box[(size_t) o].getCentreX(), outY, 1.6f);
            minX = jmin (minX, box[(size_t) o].getCentreX());
            maxX = jmax (maxX, box[(size_t) o].getCentreX());
        }
    g.setColour (t.accent);
    g.drawLine (minX - 6, outY, maxX + 6, outY, 2.5f);
    g.setFont (Font (FontOptions (10.0f, Font::bold)));
    g.drawText ("OUT", Rectangle<float> (maxX + 10, outY - 7, 30, 14), Justification::centredLeft);

    // feedback loop on op 4
    const auto& b4 = box[3];
    Path fb;
    fb.startNewSubPath (b4.getRight(), b4.getCentreY() + 4);
    fb.lineTo (b4.getRight() + 9, b4.getCentreY() + 4);
    fb.lineTo (b4.getRight() + 9, b4.getY() - 6);
    fb.lineTo (b4.getCentreX(), b4.getY() - 6);
    fb.lineTo (b4.getCentreX(), b4.getY());
    g.setColour (t.accent2.withAlpha (0.25f + 0.75f * feedback));
    g.strokePath (fb, PathStrokeType (1.5f));

    for (int o = 0; o < df::kNumOps; ++o)
    {
        const bool car = al.carrier[(size_t) o];
        g.setColour (car ? t.accent : t.accent2);
        g.fillRoundedRectangle (box[(size_t) o], 4.0f);
        g.setColour (t.bg);
        g.setFont (Font (FontOptions (13.0f, Font::bold)));
        g.drawText (String (o + 1), box[(size_t) o], Justification::centred);
    }
}

// ---------------------------------------------------------------- Envelope view

void EnvelopeView::paint (Graphics& g)
{
    if (theme == nullptr) return;
    const auto& t = *theme;
    auto r = getLocalBounds().toFloat();
    g.setColour (t.bg.withAlpha (0.5f));
    g.fillRoundedRectangle (r, 6.0f);
    r = r.reduced (6, 8);

    const float total = jmax (50.0f, p.attackMs + p.holdMs + p.decayMs);
    auto xAt = [&] (float ms) { return r.getX() + r.getWidth() * jmin (1.0f, ms / total); };
    auto yAt = [&] (float a) { return r.getBottom() - r.getHeight() * a; };

    // pitch envelope (dashed, accent2)
    if (p.pitchEnvSemis > 0.1f && p.fmLevel > 0.0f)
    {
        Path pe;
        const float amt = jmin (1.0f, p.pitchEnvSemis / 48.0f);
        for (int i = 0; i <= 60; ++i)
        {
            const float ms = total * i / 60.0f;
            const float v = amt * std::exp (-ms / jmax (1.0f, p.pitchEnvMs) * 6.9f);
            i == 0 ? pe.startNewSubPath (xAt (ms), yAt (v)) : pe.lineTo (xAt (ms), yAt (v));
        }
        Path dashed;
        const float dashes[] = { 4.0f, 3.0f };
        PathStrokeType (1.2f).createDashedStroke (dashed, pe, dashes, 2);
        g.setColour (t.accent2);
        g.fillPath (dashed);
    }

    // amp AHD
    Path amp;
    amp.startNewSubPath (xAt (0), yAt (0));
    amp.lineTo (xAt (p.attackMs), yAt (1));
    amp.lineTo (xAt (p.attackMs + p.holdMs), yAt (1));
    for (int i = 1; i <= 50; ++i)
    {
        const float ms = p.decayMs * i / 50.0f;
        amp.lineTo (xAt (p.attackMs + p.holdMs + ms), yAt (std::exp (-ms / p.decayMs * 6.9f)));
    }
    Path fill (amp);
    fill.lineTo (xAt (total), yAt (0));
    fill.closeSubPath();
    g.setColour (t.accent.withAlpha (0.18f));
    g.fillPath (fill);
    g.setColour (t.accent);
    g.strokePath (amp, PathStrokeType (1.8f));

    g.setColour (t.dim);
    g.setFont (Font (FontOptions (10.0f)));
    g.drawText ("AMP  " + String (roundToInt (total)) + " ms", getLocalBounds().reduced (6, 2), Justification::topRight);
    if (p.pitchEnvSemis > 0.1f && p.fmLevel > 0.0f)
    {
        g.setColour (t.accent2);
        g.drawText ("PITCH", getLocalBounds().reduced (6, 2), Justification::topLeft);
    }
}

// ---------------------------------------------------------------- Editor

DrumFoundryEditor::DrumFoundryEditor (DrumFoundryProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      crushAtt (p.apvts, "crush", crushKnob.slider), outputAtt (p.apvts, "output", outputKnob.slider)
{
    setLookAndFeel (&lnf);
    lnf.setTheme (themeByName (proc.getThemeName()));
    selected = proc.getSelectedPad();

    for (int i = 0; i < df::kNumPads; ++i)
    {
        pads.push_back (std::make_unique<PadButton> (*this, i));
        pads.back()->lit = proc.padLevel (i);
        addAndMakeVisible (*pads.back());
    }
    for (auto& d : kKnobDefs)
    {
        knobs.push_back (std::make_unique<Knob> (d[1], d[0], String (d[2]) == "b"));
        addAndMakeVisible (*knobs.back());
    }
    addAndMakeVisible (crushKnob);
    addAndMakeVisible (outputKnob);

    StringArray algs;
    for (int a = 0; a < df::kNumAlgorithms; ++a) algs.add (String (a + 1) + ": " + df::algorithm (a).name);
    algoBox.addItemList (algs, 1);
    for (auto& s : df::transientSet()) sampleBox.addItem (s.name, sampleBox.getNumItems() + 1);
    filterBox.addItemList ({ "Low-pass", "Band-pass", "High-pass" }, 1);
    chokeBox.addItemList ({ "Choke: Off", "Choke: 1", "Choke: 2", "Choke: 3", "Choke: 4" }, 1);
    outBox.addItem ("Out: Main", 1);
    for (int i = 1; i <= 8; ++i) outBox.addItem ("Out: " + String (i), i + 1);
    for (auto* c : { &algoBox, &sampleBox, &filterBox, &chokeBox, &outBox, &kitBox, &themeBox })
        addAndMakeVisible (*c);

    for (int i = 0; i < proc.getNumPrograms(); ++i) kitBox.addItem (String (i + 1).paddedLeft ('0', 2) + "  " + proc.getProgramName (i), i + 1);
    kitBox.onChange = [this] {
        const int k = kitBox.getSelectedItemIndex();
        if (k >= 0 && k != proc.getCurrentProgram()) { proc.setCurrentProgram (k); repaint(); }
    };
    prevKit.onClick = [this] { proc.setCurrentProgram ((proc.getCurrentProgram() + proc.getNumPrograms() - 1) % proc.getNumPrograms()); refreshKitBox(); };
    nextKit.onClick = [this] { proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % proc.getNumPrograms()); refreshKitBox(); };
    addAndMakeVisible (prevKit);
    addAndMakeVisible (nextKit);
    refreshKitBox();

    for (auto& t : allThemes()) themeBox.addItem (t.name, themeBox.getNumItems() + 1);
    themeBox.setText (lnf.theme().name, dontSendNotification);
    themeBox.onChange = [this] { applyTheme (themeBox.getText()); };

    presetButton.onClick = [this] { showPadMenu (selected); };
    addAndMakeVisible (presetButton);

    algoView.theme = &lnf.theme();
    envView.theme = &lnf.theme();
    algoView.setInterceptsMouseClicks (false, false);
    envView.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (algoView);
    addAndMakeVisible (envView);

    attachPad();
    setSize (kW, kH);
    startTimerHz (30);
}

DrumFoundryEditor::~DrumFoundryEditor()
{
    stopTimer();
    sliderAtt.clear();
    comboAtt.clear();
    setLookAndFeel (nullptr);
}

void DrumFoundryEditor::refreshKitBox()
{
    kitBox.setSelectedItemIndex (proc.getCurrentProgram(), dontSendNotification);
    repaint();
}

void DrumFoundryEditor::applyTheme (const String& name)
{
    lnf.setTheme (themeByName (name));
    proc.setThemeName (lnf.theme().name);
    themeBox.setText (lnf.theme().name, dontSendNotification);
    algoView.theme = &lnf.theme();
    envView.theme = &lnf.theme();
    sendLookAndFeelChange();
    repaint();
}

void DrumFoundryEditor::attachPad()
{
    sliderAtt.clear();
    comboAtt.clear();
    auto& apvts = proc.apvts;
    for (auto& k : knobs)
        sliderAtt.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (apvts, DrumFoundryProcessor::pid (selected, k->suffix), k->slider));
    // note knob shows note names
    for (auto& k : knobs)
        if (k->suffix == "note")
            k->slider.textFromValueFunction = [] (double v) { return noteName ((int) v) + " " + String ((int) v); };
    auto combo = [&] (ComboBox& c, const char* id) {
        comboAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, DrumFoundryProcessor::pid (selected, id), c));
    };
    combo (algoBox, "alg");
    combo (sampleBox, "sample");
    combo (filterBox, "filter");
    combo (chokeBox, "choke");
    combo (outBox, "out");
    for (auto& k : knobs) k->slider.updateText();
    refreshViews();
}

void DrumFoundryEditor::selectPad (int pad)
{
    if (pad == selected) return;
    selected = pad;
    proc.setSelectedPad (pad);
    attachPad();
    repaint();
}

void DrumFoundryEditor::showPadMenu (int pad)
{
    PopupMenu presets;
    const auto& vp = df::voicePresets();
    for (int i = 0; i < (int) vp.size(); ++i) presets.addItem (1000 + i, vp[(size_t) i].name);
    PopupMenu copyTo;
    for (int i = 0; i < df::kNumPads; ++i) if (i != pad) copyTo.addItem (2000 + i, df::padName (i));

    PopupMenu m;
    m.addSectionHeader (String ("Pad ") + String (pad + 1) + ": " + df::padName (pad));
    m.addItem (1, "Audition");
    m.addSubMenu ("Load voice preset", presets);
    m.addSubMenu ("Copy sound to", copyTo);
    m.addItem (2, "Reset to kit sound");
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (pads[(size_t) pad].get()), [this, pad] (int r) {
        if (r == 1) proc.auditionPad (pad);
        else if (r == 2) proc.resetPadToKit (pad);
        else if (r >= 2000) proc.copyPad (pad, r - 2000);
        else if (r >= 1000) { proc.loadVoicePreset (r - 1000, pad); proc.auditionPad (pad); }
        repaint();
    });
}

void DrumFoundryEditor::timerCallback()
{
    for (auto& pb : pads)
    {
        const float l = proc.padLevel (pb->pad);
        if (std::abs (l - pb->lit) > 0.01f) { pb->lit = l; pb->repaint(); }
    }
    refreshViews();
    if (kitBox.getSelectedItemIndex() != proc.getCurrentProgram()) refreshKitBox();
    repaint (lcdArea);
}

void DrumFoundryEditor::refreshViews()
{
    const auto p = proc.readPad (selected);
    if (p.algorithm != algoView.algorithm || std::abs (p.feedback - algoView.feedback) > 0.01f)
    {
        algoView.algorithm = p.algorithm;
        algoView.feedback = p.feedback;
        algoView.repaint();
    }
    envView.p = p;
    envView.repaint();
}

Rectangle<int> DrumFoundryEditor::panel (Rectangle<int> r, const String& title)
{
    panels.push_back ({ r, title });
    return r.withTrimmedTop (24).reduced (8, 4);
}

void DrumFoundryEditor::resized()
{
    panels.clear();
    auto knob = [this] (const char* suffix) -> Knob& {
        for (auto& k : knobs) if (k->suffix == suffix) return *k;
        jassertfalse;
        return *knobs.front();
    };

    // ---- top bar
    titleArea = { 16, 10, 200, 44 };
    lcdArea = { 224, 10, 330, 44 };
    kitBox.setBounds (570, 20, 230, 24);
    prevKit.setBounds (804, 20, 26, 24);
    nextKit.setBounds (832, 20, 26, 24);
    themeBox.setBounds (868, 20, 120, 24);
    crushKnob.setBounds (994, 2, 58, 62);
    outputKnob.setBounds (1052, 2, 58, 62);

    // ---- pads (left)
    const int padX = 16, padY = 78, padS = 94, gap = 8;
    for (int i = 0; i < df::kNumPads; ++i)
        pads[(size_t) i]->setBounds (padX + (i % 4) * (padS + gap), padY + (i / 4) * (padS + gap), padS, padS);

    auto out = panel ({ 16, 292, 400, 232 }, "PAD OUT & ROUTING");
    auto row = out.removeFromTop (kKnobH);
    knob ("level").setBounds (row.removeFromLeft (kKnobW));
    knob ("pan").setBounds (row.removeFromLeft (kKnobW));
    knob ("note").setBounds (row.removeFromLeft (kKnobW + 10));
    row.removeFromLeft (10);
    chokeBox.setBounds (row.removeFromTop (24).withWidth (130));
    row.removeFromTop (8);
    outBox.setBounds (row.removeFromTop (24).withWidth (130));
    out.removeFromTop (10);
    presetButton.setBounds (out.removeFromTop (26).withWidth (150));

    // ---- FM (right, top)
    auto fm = panel ({ 432, 70, 672, 274 }, "FM ENGINE  /  4 OPERATORS");
    auto left = fm.removeFromLeft (258);
    algoBox.setBounds (left.removeFromTop (24));
    left.removeFromTop (6);
    auto knobRow = left.removeFromBottom (kKnobH);
    algoView.setBounds (left.reduced (0, 2));
    for (auto* s : { "pitch", "pitchenv", "pitchenv_time", "fm_level" })
        knob (s).setBounds (knobRow.removeFromLeft (64));
    fm.removeFromLeft (14);
    const int colW = 76;
    for (int o = 0; o < 4; ++o)
    {
        auto col = fm.withX (fm.getX() + o * colW).withWidth (colW);
        opHeaders[(size_t) o] = col.removeFromTop (16);
        const String n = "op" + String (o + 1) + "_";
        knob ((n + "ratio").toRawUTF8()).setBounds (col.removeFromTop (kKnobH).withSizeKeepingCentre (kKnobW, kKnobH));
        knob ((n + "level").toRawUTF8()).setBounds (col.removeFromTop (kKnobH).withSizeKeepingCentre (kKnobW, kKnobH));
        knob ((n + "decay").toRawUTF8()).setBounds (col.removeFromTop (kKnobH).withSizeKeepingCentre (kKnobW, kKnobH));
    }
    knob ("feedback").setBounds (fm.getX() + 4 * colW, fm.getY() + 16, kKnobW, kKnobH);

    // ---- transient + noise
    auto tr = panel ({ 432, 352, 438, 112 }, "TRANSIENT  /  CRUNCH");
    auto trCol = tr.removeFromLeft (124);
    sampleBox.setBounds (trCol.removeFromTop (24));
    for (auto* s : { "start", "smp_level", "smp_pitch", "bits", "rate" })
        knob (s).setBounds (tr.removeFromLeft (kKnobW));
    auto nz = panel ({ 878, 352, 226, 112 }, "NOISE");
    for (auto* s : { "noise", "noise_colour", "noise_decay" })
        knob (s).setBounds (nz.removeFromLeft (70).withSizeKeepingCentre (kKnobW, kKnobH));

    // ---- shape
    auto sh = panel ({ 432, 472, 672, 112 }, "SHAPE  /  FILTER  /  AMP");
    auto shCol = sh.removeFromLeft (110);
    filterBox.setBounds (shCol.removeFromTop (24));
    for (auto* s : { "cutoff", "reso", "drive", "attack", "hold", "decay" })
        knob (s).setBounds (sh.removeFromLeft (kKnobW));
    sh.removeFromLeft (8);
    envView.setBounds (sh.reduced (0, 2));
}

void DrumFoundryEditor::paint (Graphics& g)
{
    const auto& t = lnf.theme();
    g.fillAll (t.bg);
    if (t.brushed)
    {
        Random rng (7);
        for (int y = 0; y < getHeight(); y += 2)
        {
            g.setColour ((rng.nextBool() ? Colours::white : Colours::black).withAlpha (0.025f + rng.nextFloat() * 0.03f));
            g.drawHorizontalLine (y, 0.0f, (float) getWidth());
        }
    }

    // title
    g.setColour (t.text);
    g.setFont (Font (FontOptions (24.0f, Font::bold)));
    g.drawText ("DRUM FOUNDRY", titleArea.withHeight (28), Justification::left);
    g.setColour (t.accent);
    g.setFont (Font (FontOptions (11.0f, Font::bold)));
    g.drawText ("FM + ROM PERCUSSION SYNTH", titleArea.withTrimmedTop (28), Justification::left);

    // LCD
    {
        auto r = lcdArea.toFloat();
        g.setColour (t.lcdBg);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (t.edge);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        const auto p = proc.readPad (selected);
        const Font mono (FontOptions (Font::getDefaultMonospacedFontName(), 14.0f, Font::bold));
        g.setFont (mono);
        g.setColour (t.lcdText);
        auto in = lcdArea.reduced (10, 5);
        g.drawText ("KIT " + String (proc.getCurrentProgram() + 1).paddedLeft ('0', 2) + "  " + proc.kitName().toUpperCase(),
                    in.removeFromTop (17), Justification::left);
        g.setFont (mono.withHeight (12.0f));
        g.setColour (t.lcdText.withAlpha (0.8f));
        g.drawText (String (df::padName (selected)).toUpperCase().paddedRight (' ', 11) + String ((int) p.sampleBits) + "BIT "
                        + String (p.sampleRateKHz, 1) + "K  VOX " + String (proc.activeVoices()).paddedLeft ('0', 2) + "/16",
                    in, Justification::left);
    }

    // panels
    for (auto& [r, title] : panels)
    {
        auto rf = r.toFloat();
        if (t.bevel)
        {
            ColourGradient grad (t.panel.brighter (0.06f), rf.getX(), rf.getY(), t.panel.darker (0.08f), rf.getX(), rf.getBottom(), false);
            g.setGradientFill (grad);
        }
        else g.setColour (t.panel);
        g.fillRoundedRectangle (rf, 8.0f);
        g.setColour (t.edge);
        g.drawRoundedRectangle (rf.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (t.accent);
        g.setFont (Font (FontOptions (11.5f, Font::bold)));
        g.drawText (title, r.withHeight (24).reduced (10, 0), Justification::centredLeft);
    }
    // selected pad name on the FM panel header
    g.setColour (t.text);
    g.setFont (Font (FontOptions (12.0f, Font::bold)));
    g.drawText (String ("PAD ") + String (selected + 1) + "  " + String (df::padName (selected)).toUpperCase(),
                Rectangle<int> (432, 70, 662, 24), Justification::centredRight);

    // op headers
    const auto& al = df::algorithm (algoView.algorithm);
    for (int o = 0; o < 4; ++o)
    {
        g.setColour (al.carrier[(size_t) o] ? t.accent : t.accent2);
        g.setFont (Font (FontOptions (11.0f, Font::bold)));
        g.drawText ("OP " + String (o + 1) + (al.carrier[(size_t) o] ? "  CAR" : "  MOD"), opHeaders[(size_t) o], Justification::centred);
    }

    // footer hint
    g.setColour (t.dim);
    g.setFont (Font (FontOptions (11.0f)));
    g.drawFittedText ("Click a pad to play and edit it. Drag one pad onto another to copy its sound. Right-click for voice presets.",
                      Rectangle<int> (16, 532, 400, 30), Justification::topLeft, 2);
    g.drawFittedText ("Multi-outs: enable Out 1-8 in your DAW; a pad sent to a disabled out plays on Main.",
                      Rectangle<int> (16, 562, 400, 30), Justification::topLeft, 2);
}
